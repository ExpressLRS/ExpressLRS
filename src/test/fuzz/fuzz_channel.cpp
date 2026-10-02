#include "fuzz_channel.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "OTA.h"

#include "fuzz_harness.h"
#include "sim_tx.h"

const char *const fateNames[] = {"deliver", "drop", "corrupt", "truncate", "forged-crc"};

bool channelSkipLoop;

static const uint8_t *input;
static size_t inputSize;
static size_t inputPos;
static unsigned dropRemaining;
static unsigned telemetryLossRemaining;
static int arrivalJitterUs;
static uint32_t jitterState;

// Takes the test case's opcodes and jitter setting
void channelStart(const uint8_t *data, size_t size, size_t firstOpcode, int jitterUs, uint32_t jitterSeed)
{
    input = data;
    inputSize = size;
    inputPos = firstOpcode;
    arrivalJitterUs = jitterUs;
    jitterState = jitterSeed;
}

// True while the test case has opcodes left
static bool inputLeft()
{
    return inputPos < inputSize;
}

// Next input byte, zero once the input is used up
static uint8_t nextByte()
{
    return inputPos < inputSize ? input[inputPos++] : 0;
}

// True while the test case still has something to play
bool channelActive()
{
    return inputLeft() || dropRemaining;
}

// How much earlier or later than nominal this packet arrives, in microseconds
int channelJitter()
{
    if (!arrivalJitterUs)
        return 0;
    jitterState = jitterState * 1664525u + 1013904223u;
    return (int)((jitterState >> 16) % (2 * arrivalJitterUs + 1)) - arrivalJitterUs;
}

// True if the TX fails to hear the telemetry packet the RX just sent
bool channelTelemetryLost()
{
    const bool lost = telemetryLossRemaining != 0;
    if (lost && telemetryLossRemaining != UINT32_MAX)
        telemetryLossRemaining--;
    return lost;
}

// Reads the next opcode: what happens to this packet
Fate channelNextFate(uint8_t *a, uint8_t *b)
{
    // One opcode per packet the RX is in a position to hear. Corrupt, truncate and forge take two more
    // bytes for position and value. Opcodes that act on the TX are applied and the next one is read.
    if (dropRemaining)
    {
        dropRemaining--;
        return DROP;
    }
    for (;;)
    {
        const uint8_t op = nextByte();
        const uint8_t arg = op >> 5;
        channelSkipLoop = (op & 0x18) == 0x18;
        switch (op & 7)
        {
        case 3:
            return DROP;
        case 4:
            *a = nextByte();
            *b = nextByte();
            return CORRUPT;
        case 5:
            *a = nextByte();
            *b = nextByte();
            return TRUNCATE;
        case 6:
            *a = nextByte();
            *b = nextByte();
            return FORGE;
        case 7:
            channelSkipLoop = false;
            switch ((op >> 3) & 3)
            {
            case 0:
                txSelectSwitchMode(arg % 3);
                break;
            case 1:
                dropRemaining = (8u << arg) - 1;
                return DROP;
            case 2:
                // The TX stops hearing the RX's telemetry: for a number of packets, or until told otherwise
                if (arg == 7)
                    telemetryLossRemaining = telemetryLossRemaining ? 0 : UINT32_MAX;
                else
                    telemetryLossRemaining = 1u << arg;
                if (fuzzTrace)
                    fprintf(stderr, "     telemetry: next %u packets lost\n", telemetryLossRemaining);
                break;
            case 3:
                txPowerCycle();
                break;
            }
            if (!inputLeft())
                return DROP;
            break;
        default:
            return DELIVER;
        }
    }
}

// Applies a corrupt, truncate or forge fate to the packet
void channelDamage(Fate fate, OTA_Packet_s *pkt, uint8_t packetSize, uint8_t a, uint8_t b)
{
    uint8_t *raw = (uint8_t *)pkt;
    switch (fate)
    {
    case CORRUPT:
    case FORGE:
        raw[a % packetSize] ^= b ? b : 1;
        break;
    case TRUNCATE:
        memset(raw + a % packetSize, (b & 1) ? 0xff : 0x00, packetSize - a % packetSize);
        break;
    default:
        break;
    }
    // A corrupted packet that happens to pass the CRC, as seen from the RX side
    if (fate == FORGE)
        OtaGeneratePacketCrc(pkt);
}

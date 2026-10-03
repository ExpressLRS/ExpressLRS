#include "fuzz_channel.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>

#include "OTA.h"

#include "fuzz_harness.h"
#include "sim_tx.h"
#include "sim_tx_ota.h"

const char *const fateNames[] = {"deliver", "drop", "corrupt", "truncate", "forged-crc"};

bool rxLoopStalled; // RX loop() stalls for the rest of this TX slot, as if behind its interrupts

static const uint8_t *input;
static size_t inputSize;
static size_t inputPos;
static unsigned dropRemaining;
static unsigned telemetryLossRemaining;
static bool telemetryLossUntilToggled;
static bool forgeUsed;
static int arrivalJitterUs;
static std::mt19937 jitterRng;

// Takes the test case's opcodes and jitter setting
void channelStart(const uint8_t *data, size_t size, size_t firstOpcode, int jitterUs, uint32_t jitterSeed)
{
    input = data;
    inputSize = size;
    inputPos = firstOpcode;
    arrivalJitterUs = jitterUs;
    jitterRng.seed(jitterSeed);
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
    return (int)(jitterRng() % (2 * arrivalJitterUs + 1)) - arrivalJitterUs;
}

// True if the TX fails to hear the telemetry packet the RX just sent
bool channelTelemetryLost()
{
    const bool lost = telemetryLossUntilToggled || telemetryLossRemaining;
    if (telemetryLossRemaining)
        telemetryLossRemaining--;
    return lost;
}

// An opcode is one input byte. Bits 0-2 give what happens to the packet, or say that it is an event:
//   0-2  deliver        3  drop        4  corrupt        5  truncate        6  forge        7  event
// For a packet, bits 3 and 4 both set means the RX's loop() does not run this slot.
// For an event, bits 3-4 pick the event and bits 5-7 are its argument.
enum class PacketOp : uint8_t
{
    DROP = 3,
    CORRUPT = 4,
    TRUNCATE = 5,
    FORGE = 6,
    EVENT = 7,
};

enum class EventOp : uint8_t
{
    UNUSED = 0,
    DROP_BURST = 1,
    TELEMETRY_LOSS = 2,
    TX_POWER_CYCLE = 3,
};

constexpr unsigned MIN_DROP_BURST = 8;         // packets, doubled per step of the argument
constexpr uint8_t TELEMETRY_LOSS_TOGGLE = 7;   // argument that switches lasting telemetry loss on or off

// What happens to the packet, or PacketOp::EVENT
static PacketOp opPacket(uint8_t op)
{
    return (PacketOp)(op & 7);
}

// Which event, for PacketOp::EVENT
static EventOp opEvent(uint8_t op)
{
    return (EventOp)((op >> 3) & 3);
}

// The event's argument, 0-7
static uint8_t opArg(uint8_t op)
{
    return op >> 5;
}

// True if the RX's loop() stalls for this slot
static bool opStallsLoop(uint8_t op)
{
    constexpr uint8_t STALL_BITS = (1 << 3) | (1 << 4);
    return (op & STALL_BITS) == STALL_BITS;
}

// Reads where in the packet the damage goes and what it is
static void readDamage(Damage *damage)
{
    damage->position = nextByte();
    damage->value = nextByte();
}

// Applies an event to the TX or the link. Returns true if the packet is dropped by it.
static bool applyEvent(uint8_t op)
{
    const uint8_t arg = opArg(op);
    switch (opEvent(op))
    {
    case EventOp::UNUSED:
        // A TX only takes a switch mode change while disconnected, so the fuzzer does not make one
        break;
    case EventOp::DROP_BURST:
        // 8 to 1024 packets, this one included
        dropRemaining = (MIN_DROP_BURST << arg) - 1;
        return true;
    case EventOp::TELEMETRY_LOSS:
        // The TX stops hearing the RX's telemetry: for a number of packets, or until told otherwise
        if (arg == TELEMETRY_LOSS_TOGGLE)
        {
            telemetryLossUntilToggled = !telemetryLossUntilToggled && !telemetryLossRemaining;
            telemetryLossRemaining = 0;
        }
        else
        {
            telemetryLossUntilToggled = false;
            telemetryLossRemaining = 1u << arg;
        }
        if (fuzzTrace && telemetryLossUntilToggled)
            fprintf(stderr, "     telemetry: lost from here on\n");
        else if (fuzzTrace)
            fprintf(stderr, "     telemetry: next %u packets lost\n", telemetryLossRemaining);
        break;
    case EventOp::TX_POWER_CYCLE:
        txPowerCycle();
        break;
    }
    return false;
}

// Reads opcodes until one says what happens to this packet. Events on the way are applied.
Fate channelNextFate(Damage *damage)
{
    if (dropRemaining)
    {
        dropRemaining--;
        return DROP;
    }
    for (;;)
    {
        const uint8_t op = nextByte();
        rxLoopStalled = opStallsLoop(op);
        switch (opPacket(op))
        {
        case PacketOp::DROP:
            return DROP;
        case PacketOp::CORRUPT:
            readDamage(damage);
            return CORRUPT;
        case PacketOp::TRUNCATE:
            readDamage(damage);
            return TRUNCATE;
        case PacketOp::FORGE:
            readDamage(damage);
            // A corrupted packet passing the CRC is a rare event, so a test case gets one
            if (forgeUsed)
                return CORRUPT;
            forgeUsed = true;
            return FORGE;
        case PacketOp::EVENT:
            rxLoopStalled = false;
            if (applyEvent(op))
                return DROP;
            if (!inputLeft())
                return DROP;
            break;
        default:
            return DELIVER;
        }
    }
}

// Applies a corrupt, truncate or forge fate to the packet
void channelDamage(Fate fate, OTA_Packet_s *pkt, uint8_t packetSize, const Damage &damage)
{
    uint8_t *raw = (uint8_t *)pkt;
    const uint8_t start = damage.position % packetSize;
    switch (fate)
    {
    case CORRUPT:
    case FORGE:
        raw[start] ^= damage.value ? damage.value : 1;
        break;
    case TRUNCATE:
        memset(raw + start, (damage.value & 1) ? 0xff : 0x00, packetSize - start);
        break;
    default:
        break;
    }
    // A corrupted packet that happens to pass the CRC with the nonce the RX is at
    if (fate == FORGE)
    {
        txOtaSelect(tx.mode, packetSize, OtaNonce);
        txOtaAddCrc(pkt);
    }
}

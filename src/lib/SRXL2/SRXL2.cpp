#include "SRXL2.h"
#include "crc.h"
#include <cstring>
#include <climits>

namespace SRXL2
{
static constexpr uint32_t CONTROL_US = 20000, FADE_US = 40000, SAMPLE_US = 100000, REQUEST_US = 200000;
static constexpr uint32_t IDLE_US = 174, RESPONSE_US = 20000, DISCOVERY_US = 50000;
static constexpr uint32_t STALE_US = 2000000, CELL_STALE_US = 5000000;

uint16_t crc16(uint8_t *bytes, uint8_t length)
{
    static Crc2Byte crc{};
    static bool initialized = false;
    if (!initialized) { crc.init(16, 0x1021); initialized = true; }
    return crc.calc(bytes, length, 0);
}

uint16_t encodeThrottle(uint16_t value)
{
    if (value <= 172) return 0x2AA0;
    if (value >= 1811) return 0xD554;
    const uint32_t mapped = value <= 992
        ? 0x2AA0u + uint32_t(value - 172) * (0x8000 - 0x2AA0) / 820
        : 0x8000u + uint32_t(value - 992) * (0xD554 - 0x8000) / 819;
    return mapped & 0xFFFC;
}

static uint16_t be16(const uint8_t *p) { return uint16_t(p[0]) << 8 | p[1]; }
static uint16_t le16(const uint8_t *p) { return uint16_t(p[1]) << 8 | p[0]; }
static uint32_t le32(const uint8_t *p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
static void reading(Reading &r, uint32_t value, uint32_t now, bool valid)
{
    r = {valid ? int32_t(value) : 0, now, valid && value <= INT32_MAX};
}
static void finish(Packet &p)
{
    const uint16_t crc = crc16(p.bytes, p.length - 2);
    p.bytes[p.length - 2] = crc >> 8;
    p.bytes[p.length - 1] = crc;
}

void Link::reset(uint32_t deviceUid, uint32_t now, bool acknowledgeOnly)
{
    *this = Link();
    uid = deviceUid;
    startupAckOnly = acknowledgeOnly;
    started = lastHello = lastBus = now;
}

void Link::restart(uint32_t now)
{
    phase = Discover;
    released = hasSample = waitingReply = cellCountKnown = monitorReplies = false;
    data = {};
    throttle = 0x8000;
    fadePending = false;
    urgentControl = false;
    // The bus has already been initialized; only first boot needs the 50 ms listen.
    started = lastHello = now - DISCOVERY_US;
    helloPending = true;
}

void Link::setControlPermission(bool allowed)
{
    if (permitted && !allowed) { controlPending = urgentControl = true; fadePending = false; }
    permitted = allowed;
    if (!allowed) { released = hasSample = monitorReplies = false; throttle = 0x8000; }
}

void Link::setThrottle(uint16_t value, uint32_t now)
{
    if (!permitted || phase != Active) return;
    if (hasSample && uint32_t(now - lastSample) >= SAMPLE_US)
    {
        released = monitorReplies = false;
        urgentControl = true;
    }
    lastSample = now;
    hasSample = true;
    controlPending = true;
    fadePending = false;
    if (!released && value >= 976 && value <= 1008)
    {
        released = true;
        throttle = 0x8000;
    }
    else throttle = released ? encodeThrottle(value) : 0x8000;
}

bool Link::connected() const { return phase == Active; }

void Link::missedFrame()
{
    if (phase != Active || controlPending) return;
    controlPending = fadePending = true;
}

bool Link::nextPacket(uint32_t now, Packet &p)
{
    const bool sampleExpired = hasSample && uint32_t(now - lastSample) >= SAMPLE_US;
    if (sampleExpired) monitorReplies = false;
    const bool peerExpired = phase == Active && monitorReplies && uint32_t(now - lastReply) >= STALE_US;
    if (sampleExpired || peerExpired)
    {
        released = hasSample = false;
        throttle = 0x8000;
        controlPending = urgentControl = true;
        fadePending = false;
    }
    if (txBusy) return false;
    if (waitingReply)
    {
        if (uint32_t(now - txEnded) < RESPONSE_US) return false;
        waitingReply = false;
    }
    if (peerExpired) restart(now);
    if (inputSize && uint32_t(now - lastByte) >= 2500) inputSize = 0;
    if (inputSize || uint32_t(now - lastBus) < IDLE_US) return false;
    if (probePending)
    {
        probePending = false;
        restart(now);
        phase = Acknowledge; // Explicit diagnostic handshake; no received ESC packet is invented.
    }
    if (startupAckOnly && phase != Acknowledge) return false;
    p = {};
    broadcastTx = false;
    if (phase != Active)
    {
        if (phase == Discover && !helloPending &&
            (uint32_t(now - started) < DISCOVERY_US || uint32_t(now - lastHello) < DISCOVERY_US)) return false;
        p.length = 14;
        const uint8_t header[] = {0xA6,0x21,14,0x21, uint8_t(phase == Broadcast ? 0xFF : 0x40),10,0,3};
        std::memcpy(p.bytes, header, sizeof(header));
        for (unsigned i = 0; i < 4; ++i) p.bytes[8 + i] = uid >> (8 * i);
        requestReply = phase != Broadcast;
        broadcastTx = phase == Broadcast;
        lastHello = now;
        helloPending = false;
    }
    else
    {
        const bool failsafe = !permitted || !hasSample;
        const bool safe = failsafe || !released;
        const bool urgentNeutral = urgentControl && (safe || throttle == 0x8000);
        const uint32_t interval = controlPending || failsafe ? CONTROL_US : FADE_US;
        if (!urgentNeutral && uint32_t(now - lastControl) < interval) return false;
        const bool fade = !safe && (fadePending || !controlPending);
        controlPending = fadePending = urgentControl = false;
        requestReply = !failsafe && (!monitorReplies || uint32_t(now - lastRequest) >= REQUEST_US);
        if (failsafe) monitorReplies = false;
        else if (requestReply && !monitorReplies)
        {
            // Start the reply timeout with an actual telemetry poll, not startup failsafe.
            monitorReplies = true;
            lastReply = now;
        }
        p.length = fade ? 14 : 16;
        const uint8_t header[] = {0xA6,0xCD,p.length,uint8_t(failsafe ? 1 : 0),uint8_t(requestReply ? 0x40 : 0),uint8_t(failsafe ? 0 : 100),0,0,uint8_t(fade ? 0 : 1),0,0,0};
        std::memcpy(p.bytes, header, sizeof(header));
        if (!fade)
        {
            const uint16_t channel = safe ? 0x8000 : throttle;
            p.bytes[12] = channel;
            p.bytes[13] = channel >> 8;
        }
        lastControl = now;
        if (requestReply) lastRequest = now;
    }
    finish(p);
    txBusy = true;
    return true;
}

void Link::transmitted(uint32_t now)
{
    if (!txBusy) return;
    txBusy = false;
    txEnded = lastBus = now;
    waitingReply = requestReply;
    if (phase == Acknowledge) phase = Broadcast;
    if (broadcastTx)
    {
        phase = Active;
        controlPending = true;
        fadePending = false;
        lastControl = now;
        urgentControl = false;
        lastRequest = now - REQUEST_US;
    }
}

void Link::resync()
{
    unsigned offset = 1;
    while (offset < inputSize && input[offset] != 0xA6) ++offset;
    inputSize -= offset;
    std::memmove(input, input + offset, inputSize);
}

void Link::receive(uint8_t byte, uint32_t now)
{
    // UART0 buffers echoes and replies. Callback gaps are not wire gaps:
    // only the idle scheduler expires partial input; receiver-source frames are ignored.
    lastByte = lastBus = now;
    if (!inputSize && byte != 0xA6) return;
    input[inputSize++] = byte;
    while (inputSize >= 3)
    {
        if (input[0] != 0xA6) { resync(); continue; }
        const uint8_t length = input[2];
        if (length < 5 || length > sizeof(input)) { resync(); continue; }
        if (inputSize < length) return;
        if (crc16(input, length - 2) != be16(input + length - 2)) { resync(); continue; }
        processFrame(now);
        inputSize -= length;
        std::memmove(input, input + length, inputSize);
    }
}

void Link::processFrame(uint32_t now)
{
    if (input[1] == 0x21 && input[2] == 14 && input[3] == 0x40)
    {
        if (startupAckOnly && input[4] != 0) return;
        if (input[4] == 0 || input[4] == 0x21)
        {
            // Match Spektrum's startup master: acknowledge before the final broadcast.
            // Discovery data is already known, so a duplicate reply is optional.
            const bool acknowledge = input[4] == 0 && (startupAckOnly || phase == Discover || phase == Acknowledge);
            const bool keepWaiting = input[4] == 0 && phase == Broadcast && waitingReply;
            restart(now);
            phase = acknowledge ? Acknowledge : Broadcast;
            waitingReply = keepWaiting;
            lastReply = now;
        }
    }
    else if (input[1] == 0x80 && input[2] == 22)
    {
        if (input[3] == 0xFF) { restart(now); return; }
        if (phase != Active || input[3] != 0x21) return;
        lastReply = now;
        data.receivedUs = now;
        waitingReply = false;
        decodeTelemetry(input + 4, now);
    }
}

void Link::decodeTelemetry(const uint8_t *p, uint32_t now)
{
    if (p[0] == 0x20)
    {
        Reading *fields[] = {&data.rpm,&data.voltage,&data.temperatureFet,&data.current,&data.temperatureBec};
        const uint16_t scale[] = {10,10,1,10,1};
        for (unsigned i = 0; i < 5; ++i)
        {
            const uint16_t raw = be16(p + 2 + 2 * i);
            reading(*fields[i], uint32_t(raw) * scale[i], now, raw != 0xFFFF);
        }
        reading(data.currentBec, uint32_t(p[12]) * 100, now, p[12] != 0xFF);
        reading(data.voltageBec, uint32_t(p[13]) * 50, now, p[13] != 0xFF);
    }
    else if (p[0] == 0x42 && (p[2] & 0x0F) == 0) // first Smart battery
    {
        const uint8_t type = p[2];
        if (type == 0 || type == 0x10 || type == 0x20 || type == 0x30)
            data.batteryTemperature = {int32_t(int8_t(p[3])) * 10, now, true};
        if (type == 0)
        {
            const uint32_t current = le32(p + 4);
            reading(data.batteryCurrent, current, now, current != 0xFFFFFFFFu && current <= INT32_MAX);
            const uint16_t capacity = le16(p + 8);
            reading(data.consumption, capacity / 10, now, capacity != 0xFFFF);
        }
        else if (type == 0x10 || type == 0x20 || type == 0x30)
        {
            const unsigned first = (type / 16 - 1) * 6;
            for (unsigned i = 0; i < 6; ++i)
            {
                const uint16_t value = le16(p + 4 + 2 * i);
                const bool valid = value != 0 && value != 0xFFFF && (!cellCountKnown || first + i < data.cellCount);
                reading(data.cells[first + i], value, now, valid);
                if (valid && !cellCountKnown && data.cellCount < first + i + 1) data.cellCount = first + i + 1;
            }
        }
        else if (type == 0x80 && p[4] > 0 && p[4] <= 18)
        {
            data.cellCount = p[4];
            cellCountKnown = true;
            for (unsigned i = data.cellCount; i < 18; ++i) data.cells[i].valid = false;
        }
    }
}

Telemetry Link::telemetry(uint32_t now) const
{
    Reading *fields[] = {&data.rpm,&data.voltage,&data.current,&data.temperatureFet,&data.temperatureBec,
        &data.voltageBec,&data.currentBec,&data.batteryCurrent,&data.consumption,&data.batteryTemperature};
    for (Reading *r : fields) r->valid &= uint32_t(now - r->updatedUs) < STALE_US;
    for (Reading &r : data.cells) r.valid &= uint32_t(now - r.updatedUs) < CELL_STALE_US;
    return data;
}
}

#include "targets.h"

#if defined(TARGET_RX)

#include "common.h"
#include "device.h"
#include "SerialIO.h"
#include "CRSF.h"

extern SerialIO *serial1IO;

static volatile bool frameAvailable = false;
static volatile bool frameMissed = false;

void ICACHE_RAM_ATTR crsfRCFrameAvailableSerial1()
{
    frameAvailable = true;
}

void ICACHE_RAM_ATTR crsfRCFrameMissedSerial1()
{
    frameMissed = true;
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int event()
{
    static connectionState_e lastConnectionState = disconnected;
    
    if (serial1IO != nullptr) 
    {
        serial1IO->setFailsafe(connectionState == disconnected && lastConnectionState == connected);
        lastConnectionState = connectionState;
    }
    return DURATION_IGNORE;
}

static bool confirmFrameAvailable()
{
    if (!frameAvailable)
        return false;
    frameAvailable = false;

    // ModelMatch failure always prevents passing the frame on
    if (!connectionHasModelMatch)
        return false;

    return true;
}

static int timeout()
{
    if (connectionState == serialUpdate || serial1IO == nullptr)
    {
        return DURATION_NEVER;  // stop callbacks when doing serial update or serial1 not used
    }

    noInterrupts();
    bool missed = frameMissed;
    frameMissed = false;
    interrupts();

    // Verify there is new ChannelData and they should be sent on
    bool sendChannels = confirmFrameAvailable();
    uint32_t duration = serial1IO->sendRCFrame(sendChannels, missed, ChannelData);

    // still get telemetry and send link stats if theres no model match
    serial1IO->processSerialInput();
    serial1IO->sendQueuedData(serial1IO->getMaxSerialWriteSize());
    return duration;
}

device_t Serial1_device = {
    .initialize = nullptr,
    .start = start,
    .event = event,
    .timeout = timeout
};

#endif
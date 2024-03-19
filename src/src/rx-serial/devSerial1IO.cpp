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

static int timeout()
{
    if (connectionState == serialUpdate || serial1IO == nullptr)
    {
        return DURATION_NEVER;  // stop callbacks when doing serial update or serial1 not used
    }

    uint32_t duration = 10; // 10ms callback (i.e. when no theres no model match)
    // only send frames if we have a model match
    if (connectionHasModelMatch)
    {
        duration = serial1IO->sendRCFrame(frameAvailable, frameMissed, ChannelData);
    }
    frameAvailable = false;
    frameMissed = false;
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
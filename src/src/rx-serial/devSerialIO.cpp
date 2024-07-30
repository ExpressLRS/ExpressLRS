#include "targets.h"

#if defined(TARGET_RX)

#include "common.h"
#include "device.h"
#include "SerialIO.h"
#include "CRSF.h"
#include "config.h"

#define NO_SERIALIO_INTERVAL 1000

extern SerialIO *serialIO;
#if defined(PLATFORM_ESP32)
extern SerialIO *serial1IO;
#endif

enum teamraceOutputInhibitState_e {
    troiPass = 0,               // Allow all packets through, normal operation
    troiDisableAwaitConfirm,    // Have received one packet with another model selected, awaiting confirm to Inhibit
    troiInhibit,                // Inhibit all output
    troiEnableAwaitConfirm,     // Have received one packet with this model selected, awaiting confirm to Pass
};

typedef struct devserial_ctx_s {
  SerialIO **io;
  bool frameAvailable;          
  bool frameMissed ;
  connectionState_e lastConnectionState;
  uint8_t lastTeamracePosition;
  teamraceOutputInhibitState_e teamraceOutputInhibitState;
} devserial_ctx_t;

static devserial_ctx_t serial0;
#if defined(PLATFORM_ESP32)
static devserial_ctx_t serial1;
#endif

void ICACHE_RAM_ATTR crsfRCFrameAvailable()
{
    serial0.frameAvailable = true;
#if defined(PLATFORM_ESP32)
    serial1.frameAvailable = true;
#endif
}

void ICACHE_RAM_ATTR crsfRCFrameMissed()
{
    serial0.frameMissed = true;
#if defined(PLATFORM_ESP32)
    serial1.frameMissed = true;
#endif
}

static int start()
{
    serial0.io = &serialIO;
    serial0.lastConnectionState = disconnected;
#if defined(PLATFORM_ESP32)
    serial1.io = &serial1IO;
    serial1.lastConnectionState = disconnected;
#endif

    return DURATION_IMMEDIATELY;
}

static int event(devserial_ctx_t *ctx)
{
    if ((*(ctx->io)) != nullptr)
    {
        if (ctx->lastConnectionState != connectionState)
        {
            (*(ctx->io))->setFailsafe(connectionState == disconnected);
        }
    }

    ctx->lastConnectionState = connectionState;

    return DURATION_IGNORE;
}

static int event0()
{
    return event(&serial0);
}

#if defined(PLATFORM_ESP32)
static int event1()
{
    return event(&serial1);
}
#endif

/***
 * @brief: Convert the current TeamraceChannel value to the appropriate config value for comparison
*/
static uint8_t teamraceChannelToConfigValue()
{
    // SWITCH3b is 1,2,3,4,5,6,x,Mid
    //             0 1 2 3 4 5    7
    // Config values are Disabled,1,2,3,Mid,4,5,6
    //                      0     1 2 3  4  5 6 7
    uint8_t retVal = CRSF_to_SWITCH3b(ChannelData[config.GetTeamraceChannel()]);
    switch (retVal)
    {
        case 0: // passthrough
        case 1: // passthrough
        case 2:
            return retVal + 1;
        case 3: // passthrough
        case 4: // passthrough
        case 5:
            return retVal + 2;
        case 7:
            return 4; // "Mid"
        default:
            // CRSF_to_SWITCH3b should only return 0-5,7 but we must return a value
            return 0;
    }
}

/***
 * @brief: Determine if FrameAvailable and it should be sent to FC
 * @return: TRUE if a new frame is available and should be processed
*/
static bool confirmFrameAvailable(devserial_ctx_t *ctx)
{
    if (!ctx->frameAvailable)
        return false;

    ctx->frameAvailable = false;

    // ModelMatch failure always prevents passing the frame on
    if (!connectionHasModelMatch)
        return false;

    constexpr uint8_t CONFIG_TEAMRACE_POS_OFF = 0;
    if (config.GetTeamracePosition() == CONFIG_TEAMRACE_POS_OFF)
    {
        ctx->teamraceOutputInhibitState = troiPass;
        return true;
    }

    // Pass the packet on if in troiPass (of course) or
    // troiDisableAwaitConfirm (keep sending channels until the teamracepos stabilizes)
    bool retVal = ctx->teamraceOutputInhibitState < troiInhibit;

    uint8_t newTeamracePosition = teamraceChannelToConfigValue();

    switch (ctx->teamraceOutputInhibitState)
    {
        case troiPass:
            // User appears to be switching away from this model, wait for confirm
            if (newTeamracePosition != config.GetTeamracePosition())
                ctx->teamraceOutputInhibitState = troiDisableAwaitConfirm;
            break;

        case troiDisableAwaitConfirm:
            // Must receive the same new position twice in a row for state to change
            if (ctx->lastTeamracePosition == newTeamracePosition)
            {
                if (newTeamracePosition != config.GetTeamracePosition())
                    ctx->teamraceOutputInhibitState = troiInhibit; // disable output
                else
                    ctx->teamraceOutputInhibitState = troiPass; // return to normal
            }
            break;

        case troiInhibit:
            // User appears to be switching to this model, wait for confirm
            if (newTeamracePosition == config.GetTeamracePosition())
                ctx->teamraceOutputInhibitState = troiEnableAwaitConfirm;
            break;

        case troiEnableAwaitConfirm:
            // Must receive the same new position twice in a row for state to change
            if (ctx->lastTeamracePosition == newTeamracePosition)
            {
                if (newTeamracePosition == config.GetTeamracePosition())
                    ctx->teamraceOutputInhibitState = troiPass; // return to normal
                else
                    ctx->teamraceOutputInhibitState = troiInhibit; // back to disabled
            }
            break;
    }

    ctx->lastTeamracePosition = newTeamracePosition;
    // troiPass or troiDisablePending indicate the model is selected still,
    // however returning true if troiDisablePending means this RX could send
    // telemetry and we do not want that
    teamraceHasModelMatch = ctx->teamraceOutputInhibitState == troiPass;
    return retVal;
}

static int timeout(devserial_ctx_t *ctx)
{
    if (*(ctx->io) == nullptr)
    {
        return NO_SERIALIO_INTERVAL;
    }

    if (connectionState == serialUpdate)
    {
        return DURATION_NEVER;  // stop callbacks when doing serial update
    }

    /***
     * TODO: This contains a problem!!
     * confirmFrameAvailable() is designed to be the thing that determines if RC frames are to be sent out
     * which includes:
     *      - No data received to be sent (interpacket delay)
     *      - Connection does not have model match
     *      - TeamRace enabled but different position selected
     * However, the SBUS IO writer doesn't go off new data coming in, it just sends data at a 9ms cadence
     * and therefore does not respect any of these conditions, relying on the one-off "failsafe" member
     * modelmatch was addressed in #2211, but resolving the merge conflict here (capnbry) re-breaks it
     *
     * Commiting this anyway though to work out a better resolution
    */

    noInterrupts();
    bool missed = ctx->frameMissed;
    ctx->frameMissed = false;
    interrupts();

    // Verify there is new ChannelData and they should be sent on
    bool sendChannels = confirmFrameAvailable(ctx);

    #if defined(MIXER)
    uint32_t duration = (*(ctx->io))->sendRCFrame(sendChannels, missed, ChannelMixedData);
    #else
    uint32_t duration = (*(ctx->io))->sendRCFrame(sendChannels, missed, ChannelData);
    #endif

    // still get telemetry and send link stats if theres no model match
    (*(ctx->io))->processSerialInput();
    (*(ctx->io))->sendQueuedData((*(ctx->io))->getMaxSerialWriteSize());
    
    return duration;
}

static int timeout0()
{
  return timeout(&serial0);
}

#if defined(PLATFORM_ESP32)
static int timeout1()
{
  return timeout(&serial1);
}
#endif

device_t Serial0_device = {
    .initialize = nullptr,
    .start = start,
    .event = event0,
    .timeout = timeout0
};

#if defined(PLATFORM_ESP32)
device_t Serial1_device = {
    .initialize = nullptr,
    .start = start,
    .event = event1,
    .timeout = timeout1
};
#endif

#endif

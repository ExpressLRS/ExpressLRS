#include "targets.h"

#if defined(TARGET_RX)

#include "common.h"
#include "device.h"
#include "SerialIO.h"
#include "CRSF.h"
#include "config.h"

extern SerialIO *serialIO;

static volatile bool frameAvailable = false;
static enum teamraceOutputInhibitState_e {
    troiPass = 0,               // Allow all packets through, normal operation
    troiDisableAwaitConfirm,    // Have received one packet with another model selected, awaiting confirm to Inhibit
    troiInhibit,                // Inhibit all output
    troiEnableAwaitConfirm,     // Have received one packet with this model selected, awaiting confirm to Pass
} teamraceOutputInhibitState;

void ICACHE_RAM_ATTR crsfRCFrameAvailable()
{
    frameAvailable = true;
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

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
static bool confirmFrameAvailable()
{
    if (!frameAvailable)
        return false;
    frameAvailable = false;

    constexpr uint8_t CONFIG_TEAMRACE_POS_OFF = 0;
    if (config.GetTeamracePosition() == CONFIG_TEAMRACE_POS_OFF)
    {
        teamraceOutputInhibitState = troiPass;
        return true;
    }

    // Pass the packet on if in troiPass (of course) or
    // troiDisableAwaitConfirm (keep sending channels until the teamracepos stabilizes)
    bool retVal = teamraceOutputInhibitState < troiInhibit;

    static uint8_t lastTeamracePosition;
    uint8_t newTeamracePosition = teamraceChannelToConfigValue();
    switch (teamraceOutputInhibitState)
    {
        case troiPass:
            // User appears to be switching away from this model, wait for confirm
            if (newTeamracePosition != config.GetTeamracePosition())
                teamraceOutputInhibitState = troiDisableAwaitConfirm;
            break;

        case troiDisableAwaitConfirm:
            // Must receive the same new position twice in a row for state to change
            if (lastTeamracePosition == newTeamracePosition)
            {
                if (newTeamracePosition != config.GetTeamracePosition())
                    teamraceOutputInhibitState = troiInhibit; // disable output
                else
                    teamraceOutputInhibitState = troiPass; // return to normal
            }
            break;

        case troiInhibit:
            // User appears to be switching to this model, wait for confirm
            if (newTeamracePosition == config.GetTeamracePosition())
                teamraceOutputInhibitState = troiEnableAwaitConfirm;
            break;

        case troiEnableAwaitConfirm:
            // Must receive the same new position twice in a row for state to change
            if (lastTeamracePosition == newTeamracePosition)
            {
                if (newTeamracePosition == config.GetTeamracePosition())
                    teamraceOutputInhibitState = troiPass; // return to normal
                else
                    teamraceOutputInhibitState = troiInhibit; // back to disabled
            }
            break;
    }

    lastTeamracePosition = newTeamracePosition;
    // troiPass or troiDisablePending indicate the model is selected still,
    // however returning true if troiDisablePending means this RX could send
    // telemetry and we do not want that
    teamraceHasModelMatch = teamraceOutputInhibitState == troiPass;
    return retVal;
}

static int timeout()
{
    if (connectionState != serialUpdate)
    {
        // Verify there is new ChannelData and they should be sent on
        bool sendChannels = confirmFrameAvailable();
        uint32_t duration = serialIO->sendRCFrameToFC(sendChannels, ChannelData);

        serialIO->handleUARTout();
        serialIO->handleUARTin();

        return duration;
    }
    return DURATION_IMMEDIATELY;
}

device_t Serial_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout
};

#endif
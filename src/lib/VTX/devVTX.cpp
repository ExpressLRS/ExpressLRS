#include "targets.h"
#include "common.h"
#include "device.h"

#include "CRSFEndpoint.h"
#include "config.h"
#include "logging.h"
#include "msp.h"

#include "devButton.h"
#include "handset.h"
#include "msptypes.h"

#define PITMODE_NOT_INITIALISED    -1
#define PITMODE_OFF                 0
#define PITMODE_ON                  1

// Period after a disconnect to wait in the VTXSS_DEBUOUNCING state so if
// we get a connection we don't resend the VTX commands.
// Needs to be long enough to reconnect, but short enough to
// reset between the user switching equipment. This is so we don't get into
// a loop of connect -> send -> write eeprom -> disconnect -> ...
// See https://github.com/ExpressLRS/ExpressLRS/issues/2976
#define VTX_DISCONNECT_DEBOUNCE_MS (1 * 1000)

static int pitmodeAuxState = PITMODE_NOT_INITIALISED;
static bool sendEepromWrite = true;

static enum VtxSendState_e
{
  VTXSS_UNKNOWN,   // Status of the remote side is unknown, so we should send immediately if connected
  VTXSS_MODIFIED,  // Config is edited, should always be sent regardless of connect state
  VTXSS_SENDING1, VTXSS_SENDING2, VTXSS_SENDING3,  VTXSS_SENDINGDONE, // Send the config 3x
  VTXSS_CONFIRMED, // Status of remote side is consistent with our config
  VTXSS_DEBOUNCING // After a disconnect, go to debouncing state so a reconnect during the debounce period won't re-send the VTX command and eeprom write and get another disconnect
} VtxSendState;

void VtxTriggerSend()
{
    VtxSendState = VTXSS_MODIFIED;
    sendEepromWrite = true;
    devicesTriggerEvent(EVENT_VTX_CHANGE);
}

void VtxPitmodeSwitchUpdate()
{
    if (config.GetVtxPitmode() <= PITMODE_ON)
    {
        pitmodeAuxState = config.GetVtxPitmode();
        return;
    }

    uint8_t auxInverted = config.GetVtxPitmode() % 2;
    uint8_t auxNumber = (config.GetVtxPitmode() / 2) + 3;
    uint8_t newPitmodeAuxState = CRSF_to_BIT(ChannelData[auxNumber]) ^ auxInverted;

    if (pitmodeAuxState == PITMODE_NOT_INITIALISED)
    {
        pitmodeAuxState = newPitmodeAuxState;
    }
    else if (pitmodeAuxState != newPitmodeAuxState)
    {
        pitmodeAuxState = newPitmodeAuxState;
        VtxTriggerSend();
        // No forced EEPROM saving of Pit Mode
        sendEepromWrite = false;
    }
}

static void eepromWriteToMSPOut()
{
    mspPacket_t packet;
    packet.reset();
    packet.function = MSP_EEPROM_WRITE;

    crsfEndpoint->AddMspMessage(&packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, CRSF_ADDRESS_CRSF_TRANSMITTER);
}

static void VtxConfigToMSPOut()
{
    DBGLN("Sending VtxConfig");
    uint8_t vtxIdx = (config.GetVtxBand()-1) * 8 + config.GetVtxChannel();

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_SET_VTX_CONFIG;
    packet.addByte(vtxIdx);     // band/channel or frequency low byte
    packet.addByte(0);          // frequency high byte, if frequency mode
    if (config.GetVtxPower())
    {
        packet.addByte(config.GetVtxPower());
        packet.addByte(pitmodeAuxState);
    }

    crsfEndpoint->AddMspMessage(&packet, CRSF_ADDRESS_FLIGHT_CONTROLLER, CRSF_ADDRESS_CRSF_TRANSMITTER);

    if (!handset->IsArmed()) // Do not send while armed.  There is no need to change the video frequency while armed.  It can also cause VRx modules to flash up their OSD menu e.g. Rapidfire.
    {
        MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
    }
}

static bool initialize()
{
    registerButtonFunction(ACTION_SEND_VTX, VtxTriggerSend);
    return true;
}

static int event()
{
    // 0 = VTX Admin disabled in the configuration
    if (config.GetVtxBand() == 0)
    {
        VtxSendState = VTXSS_CONFIRMED;
        return DURATION_NEVER;
    }

    if (VtxSendState == VTXSS_MODIFIED ||
        (VtxSendState == VTXSS_UNKNOWN && connectionState == connected))
    {
        VtxSendState = VTXSS_SENDING1;
        return 1000;
    }

    if (connectionState == disconnected && VtxSendState != VTXSS_DEBOUNCING && VtxSendState != VTXSS_UNKNOWN)
    {
        VtxSendState = VTXSS_DEBOUNCING;
        return VTX_DISCONNECT_DEBOUNCE_MS;
    }

    return DURATION_IGNORE;
}

static int timeout()
{
    if (VtxSendState == VTXSS_DEBOUNCING)
    {
        if (connectionState == disconnected)
        {
            // Still disconnected after the debounce, assume this is a new connection on next connect
            VtxSendState = VTXSS_UNKNOWN;
            sendEepromWrite = true;
        }
        else
        {
            // Reconnect within the debounce, move back to CONFIRMED
            VtxSendState = VTXSS_CONFIRMED;
        }
    }

    if (VtxSendState == VTXSS_UNKNOWN || VtxSendState == VTXSS_MODIFIED || VtxSendState == VTXSS_CONFIRMED)
    {
        return DURATION_NEVER;
    }

    VtxConfigToMSPOut();

    VtxSendState = (VtxSendState_e)((int)VtxSendState + 1);
    if (VtxSendState < VTXSS_SENDINGDONE)
    {
        return 500; // repeat send in 500ms
    }

    if (connectionState == connected)
    {
        // Connected while sending, assume the MSP got to the RX
        VtxSendState = VTXSS_CONFIRMED;
        if (sendEepromWrite)
        {
            eepromWriteToMSPOut();
        }
        sendEepromWrite = true;
    }
    else
    {
        // FIXME
        // otaConnector.ResetMspQueue();
        VtxSendState = VTXSS_UNKNOWN;
    }

    return DURATION_NEVER;
}

device_t VTX_device = {
    .initialize = initialize,
    .start = nullptr,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_CONNECTION_CHANGED | EVENT_VTX_CHANGE
};

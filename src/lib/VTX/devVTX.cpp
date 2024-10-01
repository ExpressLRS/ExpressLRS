#include "targets.h"
#include "common.h"
#include "device.h"

#include "config.h"
#include "CRSF.h"
#include "msp.h"
#include "logging.h"

#include "devButton.h"
#include "handset.h"

#define PITMODE_OFF     0
#define PITMODE_ON      1

// Delay after disconnect to preserve the VTXSS_CONFIRMED status
// Needs to be long enough to reconnect, but short enough to
// reset between the user switching equipment
#define VTX_DISCONNECT_DEBOUNCE_MS (10 * 1000)

extern Stream *TxBackpack;
static uint8_t pitmodeAuxState = 0;
static bool sendEepromWrite = true;

static enum VtxSendState_e
{
  VTXSS_UNKNOWN,   // Status of the remote side is unknown, so we should send immediately if connected
  VTXSS_MODIFIED,  // Config is editied, should always be sent regardless of connect state
  VTXSS_SENDING1, VTXSS_SENDING2, VTXSS_SENDING3,  VTXSS_SENDINGDONE, // Send the config 3x
  VTXSS_CONFIRMED  // Status of remote side is consistent with our config
} VtxSendState;

void VtxTriggerSend()
{
    VtxSendState = VTXSS_UNKNOWN;
    sendEepromWrite = true;
    devicesTriggerEvent();
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

    if (pitmodeAuxState != newPitmodeAuxState)
    {
        pitmodeAuxState = newPitmodeAuxState;
        sendEepromWrite = false;
        VtxTriggerSend();
    }
}

static void eepromWriteToMSPOut()
{
    mspPacket_t packet;
    packet.reset();
    packet.function = MSP_EEPROM_WRITE;

    CRSF::AddMspMessage(&packet, CRSF_ADDRESS_FLIGHT_CONTROLLER);
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

    CRSF::AddMspMessage(&packet, CRSF_ADDRESS_FLIGHT_CONTROLLER);

    if (!handset->IsArmed()) // Do not send while armed.  There is no need to change the video frequency while armed.  It can also cause VRx modules to flash up their OSD menu e.g. Rapidfire.
    {
        MSP::sendPacket(&packet, TxBackpack); // send to tx-backpack as MSP
    }
}

static void initialize()
{
    registerButtonFunction(ACTION_SEND_VTX, VtxTriggerSend);
}

static int event()
{
    if (VtxSendState == VTXSS_MODIFIED ||
        (VtxSendState == VTXSS_UNKNOWN && connectionState == connected))
    {
        VtxSendState = VTXSS_SENDING1;
        return 1000;
    }

    if (connectionState == disconnected)
    {
        // If the VtxSend has completed, wait before going back to VTXSS_UNKNOWN
        // to ignore a temporary disconnect after saving EEPROM
        if (VtxSendState == VTXSS_CONFIRMED)
        {
            VtxSendState = VTXSS_CONFIRMED;
            return VTX_DISCONNECT_DEBOUNCE_MS;
        }
        VtxSendState = VTXSS_UNKNOWN;
    }
    else if (VtxSendState == VTXSS_CONFIRMED && connectionState == connected)
    {
        return DURATION_NEVER;
    }

    return DURATION_IGNORE;
}

static int timeout()
{
    // 0 = off in the lua Band field
    if (config.GetVtxBand() == 0)
    {
        VtxSendState = VTXSS_CONFIRMED;
        return DURATION_NEVER;
    }

    // Can only get here in VTXSS_CONFIRMED state if still disconnected
    if (VtxSendState == VTXSS_CONFIRMED)
    {
        VtxSendState = VTXSS_UNKNOWN;
        return DURATION_NEVER;
    }

    VtxConfigToMSPOut();

    VtxSendState = (VtxSendState_e)((int)VtxSendState + 1);
    if (VtxSendState < VTXSS_SENDINGDONE)
        return 500; // repeat send in 500ms

    if (connectionState == connected)
    {
        // Connected while sending, assume the MSP got to the RX
        VtxSendState = VTXSS_CONFIRMED;
        if (sendEepromWrite)
            eepromWriteToMSPOut();
        sendEepromWrite = true;
    }
    else
    {
        VtxSendState = VTXSS_UNKNOWN;
        // Never received a connection, clear the queue which now
        // has multiple VTX config packets in it
        CRSF::ResetMspQueue();
    }

    return DURATION_NEVER;
}

device_t VTX_device = {
    .initialize = initialize,
    .start = NULL,
    .event = event,
    .timeout = timeout
};

#include "targets.h"
#include "common.h"
#include "device.h"

#include "config.h"
#include "CRSF.h"
#include "msp.h"
#include "logging.h"

extern bool ICACHE_RAM_ATTR IsArmed();
extern CRSF crsf;
extern MSP msp;
extern HardwareSerial LoggingBackpack;

static enum VtxSendState_e
{
  VTXSS_UNKNOWN,   // Status of the remote side is unknown, so we should send immediately if connected
  VTXSS_MODIFIED,  // Config is editied, should always be sent regardless of connect state
  VTXSS_SENDING1, VTXSS_SENDING2, VTXSS_SENDING3,  VTXSS_SENDINGDONE, // Send the config 3x
  VTXSS_CONFIRMED  // Status of remote side is consistent with our config
} VtxSendState;

void VtxTriggerSend()
{
    VtxSendState = VTXSS_MODIFIED;
    devicesTriggerEvent();
}

static void eepromWriteToMSPOut()
{
    mspPacket_t packet;
    packet.reset();
    packet.function = MSP_EEPROM_WRITE;

    crsf.AddMspMessage(&packet);
}

static void VtxConfigToMSPOut()
{
    DBGLN("Sending VtxConfig");
    uint8_t vtxIdx = (config.GetVtxBand()-1) * 8 + config.GetVtxChannel();

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_SET_VTX_CONFIG;
    packet.addByte(vtxIdx);
    packet.addByte(0);
    if (config.GetVtxPower()) {
        packet.addByte(config.GetVtxPower());
        packet.addByte(config.GetVtxPitmode());
    }

    crsf.AddMspMessage(&packet);
    msp.sendPacket(&packet, &LoggingBackpack); // send to tx-backpack as MSP
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
        VtxSendState = VTXSS_UNKNOWN;

    return DURATION_NEVER;
}

static int timeout()
{
    // 0 = off in the lua Band field
    // Do not send while armed
    if (config.GetVtxBand() == 0 || IsArmed())
    {
        VtxSendState = VTXSS_CONFIRMED;
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
        eepromWriteToMSPOut();
    }
    else
    {
        VtxSendState = VTXSS_UNKNOWN;
        // Never received a connection, clear the queue which now
        // has multiple VTX config packets in it
        crsf.ResetMspQueue();
    }

    return DURATION_NEVER;
}

device_t VTX_device = {
    .initialize = NULL,
    .start = NULL,
    .event = event,
    .timeout = timeout
};

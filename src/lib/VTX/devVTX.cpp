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

bool VtxConfigReadyToSend = false;

static bool VtxConfigSent = false;

static void eepromWriteToMSPOut()
{
    mspPacket_t packet;
    packet.reset();
    packet.function = MSP_EEPROM_WRITE;
    packet.addByte(0);
    packet.addByte(0);
    packet.addByte(0);
    packet.addByte(0);

    crsf.AddMspMessage(&packet);
}

static void VtxConfigToMSPOut()
{
    // 0 = off in the lua Band field
    // Do not send while armed
    if (!config.GetVtxBand() || IsArmed())
        return;

    uint8_t vtxIdx = (config.GetVtxBand()-1) * 8 + config.GetVtxChannel();

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.function = MSP_SET_VTX_CONFIG;
    packet.addByte(vtxIdx);
    packet.addByte(0);
    packet.addByte(config.GetVtxPower());
    packet.addByte(config.GetVtxPitmode());

    crsf.AddMspMessage(&packet);
    eepromWriteToMSPOut(); // FC eeprom write to save VTx setting after reboot

    msp.sendPacket(&packet, &Serial); // send to tx-backpack as MSP
}

static int event()
{
    if (VtxConfigReadyToSend && connectionState == connected)
    {
        DBGLN("Sending VTX Config, because connected");
        VtxConfigReadyToSend = false;
        VtxConfigSent = true;
        VtxConfigToMSPOut();
        return DURATION_NEVER;
    }
    else if (VtxConfigReadyToSend)
    {
        VtxConfigReadyToSend = false;
        VtxConfigSent = false;
        DBGLN("Delaying send till connected");
        return DURATION_NEVER;
    }
    else if(connectionState == connected && !VtxConfigSent)
    {
        DBGLN("Connected, lesh go");
        VtxConfigSent = true;
        return 5000; // callback in 5s to do the send
    }
    else if(connectionState == disconnected)
    {
        VtxConfigSent = false;
        return DURATION_NEVER;
    }
    return DURATION_NEVER;
}

static int timeout()
{
    DBGLN("Sending VTX Config");
    VtxConfigToMSPOut();
    return DURATION_NEVER;
}

device_t VTX_device = {
    .initialize = NULL,
    .start = NULL,
    .event = event,
    .timeout = timeout
};

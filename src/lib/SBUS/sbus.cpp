#include "sbus.h"
#include "channels.h"
#include "transport.h"

SbusRxModule sbusRxModule;

void SbusRxModule::sendRCFrameToFC(Channels* chan)
{
    if (!_dev) return;

    uint8_t outBuffer[SBUS_PACKET_SIZE] = {0};

    outBuffer[0] = SBUS_HEADER;
    memcpy(outBuffer + 1, (byte *)&chan->PackedRCdataOut, sizeof(crsf_channels_s));
    outBuffer[24] = SBUS_FOOTER;

    _dev->write(outBuffer, SBUS_PACKET_SIZE);
}

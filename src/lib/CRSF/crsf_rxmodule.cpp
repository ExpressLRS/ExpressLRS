#include "crsf_rxmodule.h"
#include "channels.h"
#include "transport.h"
#include "crc.h"

//#define DEBUG_CRSF_NO_OUTPUT // debug, don't send RC msgs over UART

extern GENERIC_CRC8 crsf_crc;
CRSF_RXModule crsfRx;

void CRSF_RXModule::begin(TransportLayer* dev)
{
  _dev = dev;
}

// Sent ASYNC
void ICACHE_RAM_ATTR CRSF_RXModule::sendLinkStatisticsToFC(Channels* chan)
{
    uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = LinkStatisticsFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(outBuffer + 3, (byte *)&chan->LinkStatistics, LinkStatisticsFrameLength);

    uint8_t crc = crsf_crc.calc(&outBuffer[2], LinkStatisticsFrameLength + 1);

    outBuffer[LinkStatisticsFrameLength + 3] = crc;

#ifndef DEBUG_CRSF_NO_OUTPUT
    if (_dev) _dev->sendAsync(outBuffer, LinkStatisticsFrameLength + 4);
#endif
}

// Sent SYNC
void ICACHE_RAM_ATTR CRSF_RXModule::sendRCFrameToFC(Channels* chan)
{
    uint8_t outBuffer[RCframeLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = RCframeLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    memcpy(outBuffer + 3, (byte *)&chan->PackedRCdataOut, RCframeLength);

    uint8_t crc = crsf_crc.calc(&outBuffer[2], RCframeLength + 1);

    outBuffer[RCframeLength + 3] = crc;
#ifndef DEBUG_CRSF_NO_OUTPUT
    if (_dev) _dev->write(outBuffer, RCframeLength + 4);
#endif
}

// Sent SYNC
void ICACHE_RAM_ATTR CRSF_RXModule::sendMSPFrameToFC(uint8_t* data)
{
    const uint8_t totalBufferLen = 14;
    if (_dev) _dev->write(data, totalBufferLen);
}

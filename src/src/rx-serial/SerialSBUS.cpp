#include "SerialSBUS.h"
#include "CRSF.h"
#include "device.h"

#define SBUS_FLAG_SIGNAL_LOSS       (1 << 2)
#define SBUS_FLAG_FAILSAFE_ACTIVE   (1 << 3)

void SerialSBUS::setLinkQualityStats(uint16_t lq, uint16_t rssi)
{
    // unsupported
}

void SerialSBUS::sendLinkStatisticsToFC()
{
    // unsupported
}

uint32_t SerialSBUS::sendRCFrameToFC(bool frameAvailable, uint32_t *channelData)
{
    crsf_channels_s PackedRCdataOut;
    PackedRCdataOut.ch0 = channelData[0];
    PackedRCdataOut.ch1 = channelData[1];
    PackedRCdataOut.ch2 = channelData[2];
    PackedRCdataOut.ch3 = channelData[3];
    PackedRCdataOut.ch4 = channelData[4];
    PackedRCdataOut.ch5 = channelData[5];
    PackedRCdataOut.ch6 = channelData[6];
    PackedRCdataOut.ch7 = channelData[7];
    PackedRCdataOut.ch8 = channelData[8];
    PackedRCdataOut.ch9 = channelData[9];
    PackedRCdataOut.ch10 = channelData[10];
    PackedRCdataOut.ch11 = channelData[11];
    PackedRCdataOut.ch12 = channelData[12];
    PackedRCdataOut.ch13 = channelData[13];
    PackedRCdataOut.ch14 = channelData[14];
    PackedRCdataOut.ch15 = channelData[15];

    uint8_t extraData = 0;
    if (!frameAvailable)
    {
        extraData |= SBUS_FLAG_SIGNAL_LOSS;
    }

    _outputPort->write(0x0F);    // HEADER
    _outputPort->write((byte *)&PackedRCdataOut, RCframeLength);
    _outputPort->write((uint8_t)extraData);    // ch 17, 18, lost packet, failsafe
    _outputPort->write((uint8_t)0x00);    // FOOTER
    return 9;   // callback is every 9ms
}

void SerialSBUS::sendMSPFrameToFC(uint8_t* data)
{
    // unsupported
}

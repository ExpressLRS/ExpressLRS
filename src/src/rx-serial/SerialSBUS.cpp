#include "SerialSBUS.h"
#include "CRSF.h"
#include "device.h"
#include "config.h"

#if defined(TARGET_RX)

#define SBUS_FLAG_SIGNAL_LOSS       (1 << 2)
#define SBUS_FLAG_FAILSAFE_ACTIVE   (1 << 3)

extern RxConfig config;

const auto SBUS_CALLBACK_INTERVAL_MS = 9;

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
    static auto sendPackets = false;
    if ((failsafe && config.GetFailsafeMode() == FAILSAFE_NO_PULSES) || (!sendPackets && connectionState != connected))
    {
        return SBUS_CALLBACK_INTERVAL_MS;
    }
    sendPackets = true;

    // TODO: if failsafeMode == FAILSAFE_SET_POSITION then we use the set positions rather than the last values
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

    // Ch 7 appears to be control mode
    // https://forum.dji.com/forum.php?mod=viewthread&tid=227897&page=2#pid2784111

    if (channelData[6] > 1300)
    {
        PackedRCdataOut.ch6 = 848;
    }
    else if (channelData[6] > 700)
    {
        PackedRCdataOut.ch6 = 512;
    }
    else
    {
        PackedRCdataOut.ch6 = 176;
    }
    
    // DBGLN("%d", PackedRCdataOut.ch4);

    uint8_t extraData = 0;
    extraData |= failsafe ? SBUS_FLAG_FAILSAFE_ACTIVE : 0;
    extraData |= frameAvailable ? 0 : SBUS_FLAG_SIGNAL_LOSS;

    _outputPort->write(0x0F);    // HEADER
    _outputPort->write((byte *)&PackedRCdataOut, RCframeLength);
    _outputPort->write((uint8_t)extraData);    // ch 17, 18, lost packet, failsafe
    _outputPort->write((uint8_t)0x00);    // FOOTER
    return SBUS_CALLBACK_INTERVAL_MS;
}

void SerialSBUS::sendMSPFrameToFC(uint8_t* data)
{
    // unsupported
}

#endif
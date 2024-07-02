#include "SerialSBUS.h"
#include "CRSF.h"
#include "device.h"
#include "config.h"

#if defined(TARGET_RX)

#define SBUS_FLAG_SIGNAL_LOSS       (1 << 2)
#define SBUS_FLAG_FAILSAFE_ACTIVE   (1 << 3)

const auto UNCONNECTED_CALLBACK_INTERVAL_MS = 10;
const auto SBUS_CALLBACK_INTERVAL_MS = 9;

uint32_t SerialSBUS::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    static auto sendPackets = false;
    bool effectivelyFailsafed = failsafe || (!connectionHasModelMatch) || (!teamraceHasModelMatch);
    if ((effectivelyFailsafed && config.GetFailsafeMode() == FAILSAFE_NO_PULSES) || (!sendPackets && connectionState != connected))
    {
        return UNCONNECTED_CALLBACK_INTERVAL_MS;
    }
    sendPackets = true;

    if ((!frameAvailable && !frameMissed && !effectivelyFailsafed) || _outputPort->availableForWrite() < 25)
    {
        return DURATION_IMMEDIATELY;
    }

    // TODO: if failsafeMode == FAILSAFE_SET_POSITION then we use the set positions rather than the last values
    crsf_channels_s PackedRCdataOut;

#if defined(PLATFORM_ESP32)
    extern Stream* serial_protocol_tx;
    extern Stream* serial1_protocol_tx;
    
    if (((config.GetSerialProtocol() == PROTOCOL_DJI_RS_PRO) && streamOut == serial_protocol_tx)||
        ((config.GetSerial1Protocol() == PROTOCOL_SERIAL1_DJI_RS_PRO) && streamOut == serial1_protocol_tx))
#else
    if (config.GetSerialProtocol() == PROTOCOL_DJI_RS_PRO)
#endif
    {
        PackedRCdataOut.ch0 = fmap(channelData[config.GetSerialChannelMap(0)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch1 = fmap(channelData[config.GetSerialChannelMap(1)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch2 = fmap(channelData[config.GetSerialChannelMap(2)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch3 = fmap(channelData[config.GetSerialChannelMap(3)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch4 = fmap(channelData[config.GetSerialChannelMap(5)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696); // Record start/stop and photo
        PackedRCdataOut.ch5 = fmap(channelData[config.GetSerialChannelMap(6)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696); // Mode
        PackedRCdataOut.ch6 = fmap(channelData[config.GetSerialChannelMap(7)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 176,  848); // Recenter and Selfie
        PackedRCdataOut.ch7 = fmap(channelData[config.GetSerialChannelMap(8)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch8 = fmap(channelData[config.GetSerialChannelMap(9)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch9 = fmap(channelData[config.GetSerialChannelMap(10)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch10 = fmap(channelData[config.GetSerialChannelMap(11)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch11 = fmap(channelData[config.GetSerialChannelMap(12)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch12 = fmap(channelData[config.GetSerialChannelMap(13)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch13 = fmap(channelData[config.GetSerialChannelMap(14)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch14 = fmap(channelData[config.GetSerialChannelMap(15)], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 352, 1696);
        PackedRCdataOut.ch15 = channelData[config.GetSerialChannelMap(4)] < CRSF_CHANNEL_VALUE_MID ? 352 : 1696;
    }
    else
    {
        PackedRCdataOut.ch0 = channelData[config.GetSerialChannelMap(0)];
        PackedRCdataOut.ch1 = channelData[config.GetSerialChannelMap(1)];
        PackedRCdataOut.ch2 = channelData[config.GetSerialChannelMap(2)];
        PackedRCdataOut.ch3 = channelData[config.GetSerialChannelMap(3)];
        PackedRCdataOut.ch4 = channelData[config.GetSerialChannelMap(4)];
        PackedRCdataOut.ch5 = channelData[config.GetSerialChannelMap(5)];
        PackedRCdataOut.ch6 = channelData[config.GetSerialChannelMap(6)];
        PackedRCdataOut.ch7 = channelData[config.GetSerialChannelMap(7)];
        PackedRCdataOut.ch8 = channelData[config.GetSerialChannelMap(8)];
        PackedRCdataOut.ch9 = channelData[config.GetSerialChannelMap(9)];
        PackedRCdataOut.ch10 = channelData[config.GetSerialChannelMap(10)];
        PackedRCdataOut.ch11 = channelData[config.GetSerialChannelMap(11)];
        PackedRCdataOut.ch12 = channelData[config.GetSerialChannelMap(12)];
        PackedRCdataOut.ch13 = channelData[config.GetSerialChannelMap(13)];
        PackedRCdataOut.ch14 = channelData[config.GetSerialChannelMap(14)];
        PackedRCdataOut.ch15 = channelData[config.GetSerialChannelMap(15)];
    }

    uint8_t extraData = 0;
    extraData |= effectivelyFailsafed ? SBUS_FLAG_FAILSAFE_ACTIVE : 0;
    extraData |= frameMissed ? SBUS_FLAG_SIGNAL_LOSS : 0;

    _outputPort->write(0x0F);    // HEADER
    _outputPort->write((byte *)&PackedRCdataOut, RCframeLength);
    _outputPort->write((uint8_t)extraData);    // ch 17, 18, lost packet, failsafe
    _outputPort->write((uint8_t)0x00);    // FOOTER
    return SBUS_CALLBACK_INTERVAL_MS;
}

#endif

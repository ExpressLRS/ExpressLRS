#include "SerialCRSF.h"
#include "CRSF.h"
#include "device.h"
#include "telemetry.h"

extern Telemetry telemetry;
extern void reset_into_bootloader();
extern void EnterBindingMode();
extern void UpdateModelMatch(uint8_t model);

void SerialCRSF::setLinkQualityStats(uint16_t lq, uint16_t rssi)
{
    linkQuality = lq;
    rssiDBM = rssi;
}

void SerialCRSF::sendLinkStatisticsToFC()
{
#if !defined(DEBUG_CRSF_NO_OUTPUT)
    if (!OPT_CRSF_RCVR_NO_SERIAL)
    {
        constexpr uint8_t outBuffer[] = {
            LinkStatisticsFrameLength + 4,
            CRSF_ADDRESS_FLIGHT_CONTROLLER,
            LinkStatisticsFrameLength + 2,
            CRSF_FRAMETYPE_LINK_STATISTICS
        };

        uint8_t crc = crsf_crc.calc(outBuffer[3]);
        crc = crsf_crc.calc((byte *)&CRSF::LinkStatistics, LinkStatisticsFrameLength, crc);

        if (_fifo.ensure(outBuffer[0] + 1))
        {
            _fifo.pushBytes(outBuffer, sizeof(outBuffer));
            _fifo.pushBytes((byte *)&CRSF::LinkStatistics, LinkStatisticsFrameLength);
            _fifo.push(crc);
        }
    }
#endif // DEBUG_CRSF_NO_OUTPUT
}

uint32_t SerialCRSF::sendRCFrameToFC(bool frameAvailable, uint32_t *channelData)
{
    if (!frameAvailable)
        return DURATION_IMMEDIATELY;

    constexpr uint8_t outBuffer[] = {
        // No need for length prefix as we aren't using the FIFO
        CRSF_ADDRESS_FLIGHT_CONTROLLER,
        RCframeLength + 2,
        CRSF_FRAMETYPE_RC_CHANNELS_PACKED
    };

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
    PackedRCdataOut.ch14 = linkQuality;
    PackedRCdataOut.ch15 = rssiDBM;

    uint8_t crc = crsf_crc.calc(outBuffer[2]);
    crc = crsf_crc.calc((byte *)&PackedRCdataOut, RCframeLength, crc);

    _outputPort->write(outBuffer, sizeof(outBuffer));
    _outputPort->write((byte *)&PackedRCdataOut, RCframeLength);
    _outputPort->write(crc);
    return DURATION_IMMEDIATELY;
}

void SerialCRSF::sendMSPFrameToFC(uint8_t* data)
{
#if !defined(DEBUG_CRSF_NO_OUTPUT)
    if (!OPT_CRSF_RCVR_NO_SERIAL)
    {
        const uint8_t totalBufferLen = CRSF_FRAME_SIZE(data[1]);
        if (totalBufferLen <= CRSF_FRAME_SIZE_MAX)
        {
            data[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
            _fifo.push(totalBufferLen);
            _fifo.pushBytes(data, totalBufferLen);
        }
    }
#endif // DEBUG_CRSF_NO_OUTPUT
}

void SerialCRSF::processByte(uint8_t byte)
{
    telemetry.RXhandleUARTin(byte);

    if (telemetry.ShouldCallBootloader())
    {
        reset_into_bootloader();
    }
    if (telemetry.ShouldCallEnterBind())
    {
        EnterBindingMode();
    }
    if (telemetry.ShouldCallUpdateModelMatch())
    {
        UpdateModelMatch(telemetry.GetUpdatedModelMatch());
    }
    if (telemetry.ShouldSendDeviceFrame())
    {
        uint8_t deviceInformation[DEVICE_INFORMATION_LENGTH];
        CRSF::GetDeviceInformation(deviceInformation, 0);
        CRSF::SetExtendedHeaderAndCrc(deviceInformation, CRSF_FRAMETYPE_DEVICE_INFO, DEVICE_INFORMATION_FRAME_SIZE, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_FLIGHT_CONTROLLER);
        sendMSPFrameToFC(deviceInformation);
    }
}

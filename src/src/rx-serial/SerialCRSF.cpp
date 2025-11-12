#include "SerialCRSF.h"

#include "OTA.h"
#include "common.h"
#include "device.h"
#include "msp2crsf.h"

extern void reset_into_bootloader();

void SerialCRSF::forwardMessage(const crsf_header_t *message)
{
    // No MSP data to the FC if team-race is selected and the correct model is not selected
    if (teamraceHasModelMatch)
    {
        auto *data = (uint8_t *)message;
        const uint8_t totalBufferLen = data[CRSF_TELEMETRY_LENGTH_INDEX] + CRSF_FRAME_NOT_COUNTED_BYTES;
        if (totalBufferLen <= CRSF_FRAME_SIZE_MAX)
        {
            // CRSF on a serial port _always_ has 0xC8 as a sync byte rather than the device_id.
            // See https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md#frame-details
            data[0] = CRSF_SYNC_BYTE;
            _fifo.lock();
            _fifo.push(totalBufferLen);
            _fifo.pushBytes(data, totalBufferLen);
            _fifo.unlock();
        }
    }
}

uint32_t SerialCRSF::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    if (!frameAvailable)
        return DURATION_IMMEDIATELY;

    crsf_channels_s PackedRCdataOut {};
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

    // In 16ch mode, do not output RSSI/LQ on channels
    if (OtaIsFullRes && OtaSwitchModeCurrent == smHybridOr16ch)
    {
        PackedRCdataOut.ch14 = channelData[14];
        PackedRCdataOut.ch15 = channelData[15];
    }
    else
    {
        // Not in 16-channel mode, send LQ and RSSI dBm
        int32_t rssiDBM = linkStats.active_antenna == 0 ? -linkStats.uplink_RSSI_1 : -linkStats.uplink_RSSI_2;

        PackedRCdataOut.ch14 = UINT10_to_CRSF(fmap(linkStats.uplink_Link_quality, 0, 100, 0, 1023));
        PackedRCdataOut.ch15 = UINT10_to_CRSF(map(constrain(rssiDBM, ExpressLRS_currAirRate_RFperfParams->RXsensitivity, -50),
                                                   ExpressLRS_currAirRate_RFperfParams->RXsensitivity, -50, 0, 1023));
    }

    constexpr uint8_t outBuffer[] = {
        // No need for length prefix as we aren't using the FIFO
        // CRSF on a serial port _always_ has 0xC8 as a sync byte rather than the device_id.
        // See https://github.com/tbs-fpv/tbs-crsf-spec/blob/main/crsf.md#frame-details
        CRSF_SYNC_BYTE,
        CRSF_FRAME_SIZE(sizeof(PackedRCdataOut)),
        CRSF_FRAMETYPE_RC_CHANNELS_PACKED
    };

    uint8_t crc = crsfRouter.crsf_crc.calc(outBuffer[2]);
    crc = crsfRouter.crsf_crc.calc((byte *)&PackedRCdataOut, sizeof(PackedRCdataOut), crc);

    _outputPort->write(outBuffer, sizeof(outBuffer));
    _outputPort->write((byte *)&PackedRCdataOut, sizeof(PackedRCdataOut));
    _outputPort->write(crc);
    return DURATION_IMMEDIATELY;
}

void SerialCRSF::processBytes(uint8_t *bytes, const uint16_t size)
{
    crsfParser.processBytes(this, bytes, size, [](const crsf_header_t *message) {
        if (message->type == CRSF_FRAMETYPE_BATTERY_SENSOR)
        {
            crsfBatterySensorDetected = true;
        }
        if (message->type == CRSF_FRAMETYPE_BARO_ALTITUDE ||
            message->type == CRSF_FRAMETYPE_VARIO)
        {
            crsfBaroSensorDetected = true;
        }
    });
}

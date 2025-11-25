#include "SerialMSP.h"
#include "CRSFRouter.h"
#include "FHSS.h"
#include "common.h"
#include "crsf_protocol.h"
#include "device.h"
#include "POWERMGNT.h"
#include <Arduino.h>

uint32_t SerialMSP::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    if (!frameAvailable && !frameMissed)
    {
        return DURATION_IMMEDIATELY;
    }

    static uint32_t lastChannelUpdate = 0;
    uint32_t currentTime = millis();

    if (currentTime - lastChannelUpdate < 20)
    {
        return DURATION_IMMEDIATELY;
    }
    lastChannelUpdate = currentTime;

    for (uint8_t i = 0; i < 16; i++)
    {
        rcChannels.rc[i] = map(channelData[i],
                               CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX,
                               1000, 2000);
    }

    injectRcChannels = true;
    injectLinkStats = true;
    injectRcInfo = true;

    return 20;
}

void SerialMSP::queueMSPFrameTransmission(uint8_t *data)
{
    msp_message_t msg;
    msp_parse_status_t status;
    MSPUtils::parseReset(&status);

    uint8_t *ptr = data;
    while (*ptr && MSPUtils::parseToMsg(&msg, &status, *ptr++))
    {
        uint16_t len = MSPUtils::msgToFrameBuf(workBuffer, &msg);
        _fifo.lock();
        if (_fifo.ensure(len))
        {
            _fifo.pushBytes(workBuffer, len);
        }
        _fifo.unlock();
        break;
    }
}

void SerialMSP::queueLinkStatisticsPacket()
{
    injectLinkStats = true;
}

void SerialMSP::sendQueuedData(uint32_t maxBytesToSend)
{
    if (injectRcChannels)
    {
        injectRcChannels = false;
        sendRcChannels(nullptr);
        return;
    }

    if (injectLinkStats && !linkStatsDisabled)
    {
        injectLinkStats = false;
        sendLinkStats();
        return;
    }

    if (injectRcInfo && !rcInfoDisabled)
    {
        injectRcInfo = false;
        sendRcInfo();
        return;
    }

    SerialIO::sendQueuedData(maxBytesToSend);
}

void SerialMSP::processBytes(uint8_t *bytes, uint16_t size)
{
    if (_fifo.available(MSP_FRAME_LEN_MAX + 16))
    {
        for (uint16_t i = 0; i < size; i++)
        {
            if (MSPUtils::parseToMsg(&serialInMsg, &serialInStatus, bytes[i]))
            {
                uint16_t len = MSPUtils::msgToFrameBuf(workBuffer, &serialInMsg);
                _fifo.lock();
                if (_fifo.ensure(len))
                {
                    _fifo.pushBytes(workBuffer, len);
                }
                _fifo.unlock();
            }
        }
    }
}

void SerialMSP::sendRcChannels(uint32_t *channelData)
{
    uint32_t currentTime = millis();
    if (currentTime - rcChannelsLastMs < 19)
    {
        return;
    }
    rcChannelsLastMs = currentTime;

    uint16_t len = MSPUtils::generateV2FrameBuf(
        workBuffer,
        MSP_TYPE_REQUEST,
        MSP_FLAG_SOURCE_ID_RC_LINK | MSP_FLAG_NO_RESPONSE,
        MSP_SET_RAW_RC,
        (uint8_t *)&rcChannels,
        MSP_SET_RAW_RC_LEN);

    noInterrupts();
    _outputPort->write(workBuffer, len);
    interrupts();
}

void SerialMSP::sendLinkStats()
{
    uint32_t currentTime = millis();
    if (currentTime - linkStatsLastMs < 200)
    {
        return;
    }
    linkStatsLastMs = currentTime;

    tMspCommonSetMspRcLinkStats payload;
    payload.sublink_id = 0;
    payload.valid_link = 1;
    payload.uplink_rssi_perc = constrain(map(linkStats.uplink_RSSI_1, -130, -30, 0, 100), 0, 100);
    payload.uplink_rssi = abs(linkStats.uplink_RSSI_1);
    payload.downlink_link_quality = 0; // linkStats.downlink_Link_quality is not known on the RX
    payload.uplink_link_quality = linkStats.uplink_Link_quality;
    payload.uplink_snr = linkStats.uplink_SNR;

    uint16_t len = MSPUtils::generateV2FrameBuf(
        workBuffer,
        MSP_TYPE_REQUEST,
        MSP_FLAG_SOURCE_ID_RC_LINK | MSP_FLAG_NO_RESPONSE,
        MSP2_COMMON_SET_MSP_RC_LINK_STATS,
        (uint8_t *)&payload,
        MSP_COMMON_SET_MSP_RC_LINK_STATS_LEN);

    noInterrupts();
    _outputPort->write(workBuffer, len);
    interrupts();
}

void SerialMSP::sendRcInfo()
{
    uint32_t currentTime = millis();
    if (currentTime - rcInfoLastMs < 2500)
    {
        return;
    }
    rcInfoLastMs = currentTime;

    tMspCommonSetMspRcInfo payload;
    payload.sublink_id = 0;
    payload.uplink_tx_power = crsfPowerToMilliwatts(linkStats.uplink_TX_Power);
    payload.downlink_tx_power = crsfPowerToMilliwatts(powerToCrsfPower(POWERMGNT::currPower()));

    memset(payload.band, 0, sizeof(payload.band));
    memset(payload.mode, 0, sizeof(payload.mode));

    // Try and calculate the band from our FHSS config

    uint32_t freqHz = FHSSconfig->freq_center;

    if (freqHz >= 400000000UL && freqHz < 500000000UL)
    {
        strcpy(payload.band, "433");
    }
    else if (freqHz >= 860000000UL && freqHz < 890000000UL)
    {
        strcpy(payload.band, "868");
    }
    else if (freqHz >= 900000000UL && freqHz < 930000000UL)
    {
        strcpy(payload.band, "915");
    }
    else if (freqHz >= 2400000000UL && freqHz < 2500000000UL)
    {
        strcpy(payload.band, "2G4");
    }
    else
    {
        strcpy(payload.band, "???");
    }

    // On LR1121, overwrite with GmX when our FHSS configs don't match

#if defined(RADIO_LR1121)
    if (FHSSconfig != FHSSconfigDualBand)
    {
        strcpy(payload.band, "GmX");
    }
#endif

    // Mode string
    uint16_t hz = 1000000 / ExpressLRS_currAirRate_Modparams->interval;
    snprintf(payload.mode, sizeof(payload.mode), "%u", hz);

    uint16_t len = MSPUtils::generateV2FrameBuf(
        workBuffer,
        MSP_TYPE_REQUEST,
        MSP_FLAG_SOURCE_ID_RC_LINK | MSP_FLAG_NO_RESPONSE,
        MSP2_COMMON_SET_MSP_RC_INFO,
        (uint8_t *)&payload,
        MSP_COMMON_SET_MSP_RC_INFO_LEN);

    noInterrupts();
    _outputPort->write(workBuffer, len);
    interrupts();
}
#include "TXModuleEndpoint.h"

#include "CRSFHandset.h"
#include "common.h"
#include "lua.h"

#if defined(PLATFORM_ESP32)
RTC_DATA_ATTR int rtcModelId = 0;
#endif

void SetSyncSpam();

TXModuleEndpoint::TXModuleEndpoint() : CRSFEndpoint(CRSF_ADDRESS_CRSF_TRANSMITTER), modelId(0)
{
}

void TXModuleEndpoint::begin()
{
#if defined(PLATFORM_ESP32)
    if (esp_reset_reason() != ESP_RST_POWERON)
    {
        modelId = rtcModelId;
        if (RecvModelUpdate) RecvModelUpdate();
    }
#endif
}

bool TXModuleEndpoint::handleRaw(const crsf_header_t *message)
{
    if (message->type == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
        RcPacketToChannelsData(message);
        return true;    // do NOT forward channel data via CRSF, as we have 'magic' OTA encoding
    }
    return false;
}

void TXModuleEndpoint::handleMessage(const crsf_header_t *message)
{
    const crsf_frame_type_e packetType = message->type;

    const auto extMessage = (crsf_ext_header_t *)message;
    // Enter Binding Mode
    if (packetType == CRSF_FRAMETYPE_COMMAND
        && extMessage->frame_size >= 6 // official CRSF has 7 bytes with two CRCs
        && extMessage->payload[0] == CRSF_COMMAND_SUBCMD_RX
        && extMessage->payload[1] == CRSF_COMMAND_SUBCMD_RX_BIND)
    {
        if (OnBindingCommand) OnBindingCommand();
    }
    else if (packetType == CRSF_FRAMETYPE_COMMAND
        && extMessage->payload[0] == CRSF_COMMAND_SUBCMD_RX
        && extMessage->payload[1] == CRSF_COMMAND_MODEL_SELECT_ID)
    {
        modelId = extMessage->payload[2];
#if defined(PLATFORM_ESP32)
        rtcModelId = modelId;
#endif
        if (RecvModelUpdate) RecvModelUpdate();
    }
    else if (packetType == CRSF_FRAMETYPE_DEVICE_PING
        || packetType == CRSF_FRAMETYPE_PARAMETER_READ
        || packetType == CRSF_FRAMETYPE_PARAMETER_WRITE)
    {
        luaParamUpdateReq(extMessage->orig_addr, packetType, extMessage->payload[0], extMessage->payload[1]);
        SetSyncSpam();
    }
}

void TXModuleEndpoint::RcPacketToChannelsData(const crsf_header_t *message) // data is packed as 11 bits per channel
{
    const auto payload = (uint8_t *)message + sizeof(crsf_header_t);
    constexpr unsigned srcBits = 11;
    constexpr unsigned dstBits = 11;
    constexpr unsigned inputChannelMask = (1 << srcBits) - 1;
    constexpr unsigned precisionShift = dstBits - srcBits;

    // code from BetaFlight rx/crsf.cpp / bitpacker_unpack
    uint8_t bitsMerged = 0;
    uint32_t readValue = 0;
    unsigned readByteIndex = 0;
    for (uint32_t & n : ChannelData)
    {
        while (bitsMerged < srcBits)
        {
            const uint8_t readByte = payload[readByteIndex++];
            readValue |= ((uint32_t) readByte) << bitsMerged;
            bitsMerged += 8;
        }
        //printf("rv=%x(%x) bm=%u\n", readValue, (readValue & inputChannelMask), bitsMerged);
        n = (readValue & inputChannelMask) << precisionShift;
        readValue >>= srcBits;
        bitsMerged -= srcBits;
    }

    handset->SetRCDataReceived();

    //
    // sends channel data and also communicates commanded armed status in arming mode Switch.
    // frame len 24 -> arming mode CH5: use channel 5 value
    // frame len 25 -> arming mode Switch: use commanded arming status in extra byte
    //
    armCmd = message->frame_size == 24 ? CRSF_to_BIT(ChannelData[4]) : payload[readByteIndex];

    // monitoring arming state
    if (lastArmCmd != armCmd)
    {
        handset->SetArmed(armCmd);
        lastArmCmd = armCmd;
#if defined(PLATFORM_ESP32)
        devicesTriggerEvent(EVENT_ARM_FLAG_CHANGED);
#endif
    }
}

#include "TXModuleCRSF.h"

#include "CRSF.h"
#include "CRSFHandset.h"
#include "common.h"
#include "msp.h"
#include "msptypes.h"

#if defined(PLATFORM_ESP32)
RTC_DATA_ATTR int rtcModelId = 0;
#endif

TXModuleCRSF::TXModuleCRSF() : CRSFEndPoint(CRSF_ADDRESS_CRSF_TRANSMITTER), modelId(0)
{
}

void TXModuleCRSF::begin()
{
#if defined(PLATFORM_ESP32)
    if (esp_reset_reason() != ESP_RST_POWERON)
    {
        modelId = rtcModelId;
        if (RecvModelUpdate) RecvModelUpdate();
    }
#endif
}

bool TXModuleCRSF::handleMessage(crsf_ext_header_t *message)
{
    const crsf_frame_type_e packetType = message->type;

    if (packetType == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
        RcPacketToChannelsData(message);
        return true;    // do NOT forward channel data via CRSF
    }

    // Enter Binding Mode
    if (packetType == CRSF_FRAMETYPE_COMMAND
        && message->frame_size >= 6 // official CRSF is 7 bytes with two CRCs
        && message->dest_addr == CRSF_ADDRESS_CRSF_TRANSMITTER
        && message->orig_addr == CRSF_ADDRESS_RADIO_TRANSMITTER
        && message->payload[0] == CRSF_COMMAND_SUBCMD_RX
        && message->payload[1] == CRSF_COMMAND_SUBCMD_RX_BIND)
    {
        if (OnBindingCommand) OnBindingCommand();
    }

    if (packetType >= CRSF_FRAMETYPE_DEVICE_PING &&
        (message->dest_addr == CRSF_ADDRESS_CRSF_TRANSMITTER || message->dest_addr == CRSF_ADDRESS_BROADCAST))
    {
        if (packetType == CRSF_FRAMETYPE_COMMAND && message->payload[0] == CRSF_COMMAND_SUBCMD_RX && message->payload[1] == CRSF_COMMAND_MODEL_SELECT_ID)
        {
            modelId = message->payload[2];
#if defined(PLATFORM_ESP32)
            rtcModelId = modelId;
#endif
            if (RecvModelUpdate) RecvModelUpdate();
        }
        else
        {
            if (RecvParameterUpdate) RecvParameterUpdate(message->orig_addr, packetType, message->payload[0], message->payload[1]);
        }
    }

    // Some types trigger a telemetry burst to attempt a connection even with telemetry off,
    // but for pings (which are sent when the user loads Lua) do not forward unless connected
    return connectionState != connected && packetType == CRSF_FRAMETYPE_DEVICE_PING;
}

void TXModuleCRSF::RcPacketToChannelsData(crsf_ext_header_t *message) // data is packed as 11 bits per channel
{
    const auto payload = ((crsf_header_t *)message)->payload;
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
    armCmd = message->frame_size == 24 ? CRSF_to_BIT(ChannelData[4]) : message->payload[25];

    // monitoring arming state
    if (lastArmCmd != armCmd) {
        handset->SetArmed(armCmd);
        lastArmCmd = armCmd;
#if defined(PLATFORM_ESP32)
        devicesTriggerEvent(EVENT_ARM_FLAG_CHANGED);
#endif
    }
}

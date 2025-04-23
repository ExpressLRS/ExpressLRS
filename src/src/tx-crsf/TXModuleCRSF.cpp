#include "TXModuleCRSF.h"

#include "common.h"

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

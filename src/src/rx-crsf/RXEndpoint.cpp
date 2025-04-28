#include "RXEndpoint.h"

#include "config.h"
#include "devVTXSPI.h"
#include "freqTable.h"
#include "logging.h"
#include "lua.h"
#include "msptypes.h"

extern void reset_into_bootloader();
extern void UpdateModelMatch(uint8_t model);

RXEndpoint::RXEndpoint()
    : CRSFEndpoint(CRSF_ADDRESS_CRSF_RECEIVER)
{
}

/**
 * Handle any non-CRSF commands that we receive
 * @param message
 * @return
 */
bool RXEndpoint::handleRaw(uint8_t *message)
{
    if (message[0] == CRSF_ADDRESS_CRSF_RECEIVER && message[CRSF_TELEMETRY_LENGTH_INDEX] == 4 && message[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_COMMAND)
    {
        // Non CRSF, dest=b src=l -> reboot to bootloader
        if (message[3] == 'b' && message[4] == 'l')
        {
            reset_into_bootloader();
            return true;
        }
        if (message[3] == 'b' && message[4] == 'd')
        {
            EnterBindingModeSafely();
            return true;
        }
        if (message[3] == 'm' && message[4] == 'm')
        {
            config.SetModelId(message[5]);
            return true;
        }
    }
    return false;
}

bool RXEndpoint::handleMessage(const crsf_header_t *message)
{
    const auto extMessage = (crsf_ext_header_t *)message;
    // 1. CRSF bind command
    if (message->type == CRSF_FRAMETYPE_COMMAND && extMessage->payload[0] == CRSF_COMMAND_SUBCMD_RX && extMessage->payload[1] == CRSF_COMMAND_SUBCMD_RX_BIND)
    {
        EnterBindingModeSafely();
        return true;
    }
    if (message->type == CRSF_FRAMETYPE_MSP_WRITE && extMessage->payload[2] == MSP_SET_RX_CONFIG && extMessage->payload[3] == MSP_ELRS_MODEL_ID)
    {
        DBGLN("Set ModelId=%u", extMessage->payload[4]);
        config.SetModelId(extMessage->payload[4]);
    }
#if defined(PLATFORM_ESP32)
    else if (message->type == CRSF_FRAMETYPE_MSP_WRITE && extMessage->payload[2] == MSP_SET_VTX_CONFIG)
    {
        if (OPT_HAS_VTX_SPI)
        {
            vtxSPIFrequency = getFreqByIdx(extMessage->payload[3]);
            if (extMessage->payload[1] >= 4) // If packet has 4 bytes it also contains power idx and pitmode.
            {
                vtxSPIPowerIdx = extMessage->payload[5];
                vtxSPIPitmode = extMessage->payload[6];
            }
            devicesTriggerEvent(EVENT_VTX_CHANGE);
        }
    }
#endif
    else if (message->type == CRSF_FRAMETYPE_DEVICE_PING ||
             message->type == CRSF_FRAMETYPE_PARAMETER_READ ||
             message->type == CRSF_FRAMETYPE_PARAMETER_WRITE)
    {
        luaParamUpdateReq(
            extMessage->orig_addr,
            extMessage->type,
            extMessage->payload[0],
            extMessage->payload[1]
        );
    }
    return false;
}
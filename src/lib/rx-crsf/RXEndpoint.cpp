#include "RXEndpoint.h"
#include "CRSFRouter.h"

#if !defined(UNIT_TEST)
#include "config.h"
#include "devMSPVTX.h"
#include "devVTXSPI.h"
#include "freqTable.h"
#include "logging.h"
#include "msptypes.h"

extern void reset_into_bootloader();

RXEndpoint::RXEndpoint()
    : CRSFEndpoint(CRSF_ADDRESS_CRSF_RECEIVER)
{
}

/**
 * Handle any non-CRSF commands that we receive
 * @param message
 * @return
 */
bool RXEndpoint::handleRaw(const crsf_header_t *message)
{
    if (message->sync_byte == CRSF_ADDRESS_CRSF_RECEIVER && message->frame_size >= 4 && message->type == CRSF_FRAMETYPE_COMMAND)
    {
        uint8_t *payload = (uint8_t *)message + sizeof(crsf_header_t);
        // Non CRSF, dest=b src=l -> reboot to bootloader
        if (payload[0] == 'b' && payload[1] == 'l')
        {
            reset_into_bootloader();
            return true;
        }
        if (payload[0] == 'b' && payload[1] == 'd')
        {
            EnterBindingModeSafely();
            return true;
        }
        if (payload[0] == 'm' && payload[1] == 'm')
        {
            config.SetModelId(payload[2]);
            return true;
        }
    }
    return false;
}

/**
 * Handles MSP_RX_CONFIG command
 */
void RXEndpoint::handleMspGetRxConfig(crsf_ext_header_t *extMessage)
{
    switch ((MSP_ELRS_RX_CONFIG)extMessage->payload[3])
    {
        case MSP_ELRS_RX_CONFIG::UID:
            {
                mspPacket_t msp;
                msp.reset();
                msp.makeResponse();
                msp.function = MSP_RX_CONFIG;
                msp.addByte((uint8_t)MSP_ELRS_RX_CONFIG::UID);
                msp.addByte(UID[0]); msp.addByte(UID[1]); msp.addByte(UID[2]);
                msp.addByte(UID[3]); msp.addByte(UID[4]); msp.addByte(UID[5]);
                crsfRouter.AddMspMessage(&msp, extMessage->orig_addr, getDeviceId());
                break;
            }

        default:
            break;
    }
}

/**
 * Handles MSP_SET_RX_CONFIG command
 */
void RXEndpoint::handleMspSetRxConfig(crsf_ext_header_t *extMessage)
{
    // Encapsulated MSP payload is 0x30, mspPayloadSize, command
    // subtracting one for the subcommand in payload[3]
    auto payloadLen = extMessage->payload[1] - 1;
    auto mspPayload = &extMessage->payload[4];

    switch ((MSP_ELRS_RX_CONFIG)extMessage->payload[3])
    {
        case MSP_ELRS_RX_CONFIG::UID:
            if (payloadLen > 5)
            {
                //DBGLN("Set UID");
                config.SetUID(mspPayload);
                config.Commit();
                scheduleRebootTime(400);
            }
            break;

        case MSP_ELRS_RX_CONFIG::BIND_PHRASE:
            if (payloadLen > 0)
            {
                #if defined(DEBUG_LOG)
                mspPayload[payloadLen] = 0; // will overwrite CRC
                DBGLN("Set bindphrase '%s'", (char *)mspPayload);
                #endif
                config.SetBindPhrase(mspPayload, payloadLen);
                config.Commit();
                scheduleRebootTime(400);
            }
            break;

        case MSP_ELRS_RX_CONFIG::MODEL_ID:
            if (payloadLen > 0)
            {
                DBGLN("Set ModelId=%u", extMessage->payload[4]);
                config.SetModelId(extMessage->payload[4]);
            }
            break;

        default:
            break;
    }
}

void RXEndpoint::handleMessage(const crsf_header_t *message)
{
    const auto extMessage = (crsf_ext_header_t *)message;
    //DBG("msg %u = %u %u %u ", extMessage->type, extMessage->payload[0], extMessage->payload[1], extMessage->payload[2]);

    if (message->type == CRSF_FRAMETYPE_COMMAND && extMessage->payload[0] == CRSF_COMMAND_SUBCMD_RX && extMessage->payload[1] == CRSF_COMMAND_SUBCMD_RX_BIND)
    {
        EnterBindingModeSafely();
    }
    else if (message->type == CRSF_FRAMETYPE_MSP_REQ && extMessage->payload[2] == MSP_RX_CONFIG)
    {
        handleMspGetRxConfig(extMessage);
    }
    else if (message->type == CRSF_FRAMETYPE_MSP_WRITE && extMessage->payload[2] == MSP_SET_RX_CONFIG)
    {
        handleMspSetRxConfig(extMessage);
    }
#if defined(PLATFORM_ESP32)
    else if (message->type == CRSF_FRAMETYPE_MSP_RESP)
    {
        mspVtxProcessPacket((uint8_t *)message);
    }
    else if (OPT_HAS_VTX_SPI && message->type == CRSF_FRAMETYPE_MSP_WRITE && extMessage->payload[2] == MSP_SET_VTX_CONFIG)
    {
        vtxSPIFrequency = getFreqByIdx(extMessage->payload[3]);
        if (extMessage->payload[1] >= 4) // If packet has 4 bytes it also contains power idx and pitmode.
        {
            vtxSPIPowerIdx = extMessage->payload[5];
            vtxSPIPitmode = extMessage->payload[6];
        }
        devicesTriggerEvent(EVENT_VTX_CHANGE);
    }
#endif
    else if (message->type == CRSF_FRAMETYPE_DEVICE_PING ||
             message->type == CRSF_FRAMETYPE_PARAMETER_READ ||
             message->type == CRSF_FRAMETYPE_PARAMETER_WRITE)
    {
        parameterUpdateReq(
            extMessage->orig_addr,
            false,
            extMessage->type,
            extMessage->payload[0],
            extMessage->payload[1]
        );
    }
}
#endif

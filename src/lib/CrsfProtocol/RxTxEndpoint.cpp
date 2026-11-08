#include "RxTxEndpoint.h"

#if !defined(UNIT_TEST)

#include "CRSFRouter.h"
#include "rxtx_intf.h"
#include "config.h"
#include "logging.h"

bool RxTxEndpoint::handleRxTxMessage(const crsf_header_t *message)
{
    const auto extMessage = (crsf_ext_header_t *)message;

    if (message->type == CRSF_FRAMETYPE_MSP_REQ && extMessage->payload[2] == MSP_ELRS_RXTX_CONFIG)
    {
        handleMspGetRxTxConfig(extMessage);
        return true;
    }
    if (message->type == CRSF_FRAMETYPE_MSP_WRITE && extMessage->payload[2] == MSP_ELRS_RXTX_CONFIG)
    {
        handleMspSetRxTxConfig(extMessage);
        return true;
    }

    return false;
}

/**
 * Handles REQ(get) of MSP_ELRS_RXTX_CONFIG command
 */
void RxTxEndpoint::handleMspGetRxTxConfig(crsf_ext_header_t *extMessage)
{
    switch ((MSP_ELRS_RXTX_CONFIG_SUBCMD)extMessage->payload[3])
    {
        case MSP_ELRS_RXTX_CONFIG_SUBCMD::UID:
            {
                mspPacket_t msp;
                msp.reset();
                msp.makeResponse();
                msp.function = MSP_ELRS_RXTX_CONFIG;
                msp.addByte((uint8_t)MSP_ELRS_RXTX_CONFIG_SUBCMD::UID);
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
 * Handles WRITE(set) of MSP_ELRS_RXTX_CONFIG command
 */
void RxTxEndpoint::handleMspSetRxTxConfig(crsf_ext_header_t *extMessage)
{
    // Encapsulated MSP header is (0x30, mspPayloadSize, command)
    // mspPayloadSize must include the subcommand in payload[3], and the subcommand's
    // data must fit within the frame: frame_size counts type, dest, orig and CRC, plus
    // payload[0..3], so the data has at most frame_size - 8 bytes
    const uint8_t mspPayloadSize = extMessage->payload[1];
    if (mspPayloadSize == 0
        || extMessage->frame_size < 8
        || extMessage->frame_size > CRSF_MAX_PACKET_LEN - CRSF_FRAME_NOT_COUNTED_BYTES
        || mspPayloadSize - 1 > extMessage->frame_size - 8)
    {
        return;
    }
    const uint8_t payloadLen = mspPayloadSize - 1;
    auto mspPayload = &extMessage->payload[4];

    switch ((MSP_ELRS_RXTX_CONFIG_SUBCMD)extMessage->payload[3])
    {
        case MSP_ELRS_RXTX_CONFIG_SUBCMD::UID:
            if (payloadLen > 5)
            {
                //DBGLN("Set UID");
                config.SetUID(mspPayload);
                scheduleRebootTime(200);
            }
            break;

        case MSP_ELRS_RXTX_CONFIG_SUBCMD::BIND_PHRASE:
            // 0 len payload supported to clear binding
            #if defined(DEBUG_LOG)
            mspPayload[payloadLen] = 0; // will overwrite CRC
            DBGLN("Set bindphrase '%s'", (char *)mspPayload);
            #endif
            config.SetBindPhrase(mspPayload, payloadLen);
            scheduleRebootTime(200);
            break;

        case MSP_ELRS_RXTX_CONFIG_SUBCMD::MODEL_ID:
            #if defined(TARGET_RX)
            if (payloadLen > 0)
            {
                DBGLN("Set ModelId=%u", extMessage->payload[4]);
                config.SetModelId(extMessage->payload[4]);
            }
            #endif
            break;

        default:
            break;
    }
}

#endif /* !UNIT_TEST */
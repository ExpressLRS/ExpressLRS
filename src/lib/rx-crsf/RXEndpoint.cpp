#include "RXEndpoint.h"

#include "config.h"
#include "crsf2msp.h"
#include "devMSPVTX.h"
#include "devVTXSPI.h"
#include "freqTable.h"
#include "logging.h"
#include "lua.h"
#include "msptypes.h"

#if defined(TARGET_RX) // enable MSP2WIFI for RX only at the moment
#include "tcpsocket.h"
extern TCPSOCKET wifi2tcp;
#endif

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
    if (message->device_addr == CRSF_ADDRESS_CRSF_RECEIVER && message->frame_size >= 4 && message->type == CRSF_FRAMETYPE_COMMAND)
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

void RXEndpoint::handleMessage(const crsf_header_t *message)
{
    const auto extMessage = (crsf_ext_header_t *)message;

    // this needs refactoring in the future; convert the TCPSocket into a CRSFConnector, and this bit would move there
    if (message->type == CRSF_FRAMETYPE_MSP_RESP || message->type == CRSF_FRAMETYPE_MSP_REQ) // if we have a client we probs wanna talk to it
    {
        DBGLN("Got MSP frame, forwarding to client");
        wifi2tcp.crsfMspIn((uint8_t *)message);
    }

    if (message->type == CRSF_FRAMETYPE_COMMAND && extMessage->payload[0] == CRSF_COMMAND_SUBCMD_RX && extMessage->payload[1] == CRSF_COMMAND_SUBCMD_RX_BIND)
    {
        EnterBindingModeSafely();
    }
    else if (message->type == CRSF_FRAMETYPE_MSP_WRITE && extMessage->payload[2] == MSP_SET_RX_CONFIG && extMessage->payload[3] == MSP_ELRS_MODEL_ID)
    {
        DBGLN("Set ModelId=%u", extMessage->payload[4]);
        config.SetModelId(extMessage->payload[4]);
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
        luaParamUpdateReq(
            extMessage->orig_addr,
            extMessage->type,
            extMessage->payload[0],
            extMessage->payload[1]
        );
    }
}
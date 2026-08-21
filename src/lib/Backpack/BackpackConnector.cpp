#include "targets.h"

#if defined(PLATFORM_ESP32) && defined(TARGET_TX)

#include "BackpackConnector.h"

#include "config.h"
#include "logging.h"

void BackpackConnector::forwardMessage(const crsf_header_t *message)
{
    // Only MSP is understood by the backpack, and broadcast messages are delivered
    // to every connector, so ignore anything that isn't MSP for the video receiver
    if (message->type != CRSF_FRAMETYPE_MSP_REQ && message->type != CRSF_FRAMETYPE_MSP_WRITE)
    {
        return;
    }
    const auto extMessage = (crsf_ext_header_t *)message;
    if (extMessage->dest_addr != CRSF_ADDRESS_VIDEO_RECEIVER || config.GetBackpackDisable())
    {
        return;
    }

    // the backpack speaks native MSP, so rebuild the (possibly chunked) MSP frame and send it on
    crsf2msp.parse((uint8_t *)message, [](const uint8_t *data, const uint32_t len) {
        BackpackOrLogStrm->write(data, len);
    });
}

#endif

#ifndef VIDEO_RECEIVER_ENDPOINT_H
#define VIDEO_RECEIVER_ENDPOINT_H

#include "CRSFEndpoint.h"
#include "msp.h"

/**
 * Decodes the MSP message encapsulated in a CRSF MSP frame.
 *
 * Both MSPv1 and MSPv2 encapsulation are understood. Only single-frame messages
 * are decoded; chunked messages are rejected rather than reassembled.
 *
 * @param message the CRSF frame carrying the encapsulated MSP message
 * @param packet receives the decoded MSP command when this returns true
 * @return true if a complete MSP message was decoded into packet
 */
bool decodeEncapsulatedMsp(const crsf_header_t *message, mspPacket_t *packet);

/**
 * Relays MSP messages addressed to the video receiver on to the backpack.
 *
 * Messages are forwarded, never interpreted, so any MSP command the backpack
 * understands can be sent from the handset without a change here.
 */
class VideoReceiverEndpoint final : public CRSFEndpoint {
public:
    VideoReceiverEndpoint() : CRSFEndpoint(CRSF_ADDRESS_VIDEO_RECEIVER) {}
    ~VideoReceiverEndpoint() override = default;

    void handleMessage(const crsf_header_t *message) override;
};

extern VideoReceiverEndpoint videoReceiver;

#endif //VIDEO_RECEIVER_ENDPOINT_H

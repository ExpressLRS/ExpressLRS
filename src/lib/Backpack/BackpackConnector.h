#pragma once

#include "CRSFConnector.h"
#include "crsf2msp.h"

/**
 * @brief Delivers CRSF messages addressed to the video receiver over the backpack link.
 *
 * The tx-backpack is the route to the `CRSF_ADDRESS_VIDEO_RECEIVER` device, it speaks
 * native MSP over its serial connection and relays what it receives to the VRx backpack
 * over ESP-NOW. Any MSP message routed to the video receiver is therefore unwrapped from
 * its CRSF frame(s) and written to the backpack as an MSP frame.
 */
class BackpackConnector final : public CRSFConnector
{
public:
    BackpackConnector() { addDevice(CRSF_ADDRESS_VIDEO_RECEIVER); }
    ~BackpackConnector() override = default;

    void forwardMessage(const crsf_header_t *message) override;

private:
    CROSSFIRE2MSP crsf2msp;
};

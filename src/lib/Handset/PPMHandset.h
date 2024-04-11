#pragma once

#include "handset.h"

#include <driver/rmt.h>

class PPMHandset final : public Handset
{
public:
    void Begin() override;
    void End() override;
    bool IsArmed() override;
    void handleInput() override;

private:
    uint32_t lastPPM = 0;
    size_t numChannels = 0;
    RingbufHandle_t rb = nullptr;

    rmt_channel_t rmtChannel;
};

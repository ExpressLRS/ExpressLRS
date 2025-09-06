#pragma once
#include "handset.h"

#include <driver/rmt.h>

class PPMHandset final : public Handset
{
public:
    void Begin() override;
    void End() override;
    void handleInput() override;

private:
    uint32_t lastPPM = 0;
    size_t numChannels = 0;
    RingbufHandle_t rb = nullptr;
};

#if defined(PLATFORM_ESP32_S3)
constexpr rmt_channel_t PPM_RMT_CHANNEL = RMT_CHANNEL_4;
#elif defined(PLATFORM_ESP32_C3)
constexpr rmt_channel_t PPM_RMT_CHANNEL = RMT_CHANNEL_2;
#else
constexpr rmt_channel_t PPM_RMT_CHANNEL = RMT_CHANNEL_0;
#endif

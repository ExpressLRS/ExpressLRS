#pragma once

#include "handset.h"

#include <driver/rmt.h>

class AutoDetect final : public Handset
{
public:
    void Begin() override;
    void End() override;
    bool IsArmed() override;
    void handleInput() override;

private:
    void migrateTo(Handset *that) const;
    void startPPM() const;
    void startCRSF() const;

    int input_detect = 0;
    RingbufHandle_t rb = nullptr;
    uint32_t lastDetect = 0;

    rmt_channel_t rmtChannel;
};
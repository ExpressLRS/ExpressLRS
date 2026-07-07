#pragma once

#include "handset.h"

#include <driver/rmt.h>

class AutoDetect final : public Handset
{
public:
    void Begin() override;
    void End() override;
    void handleInput() override;

    static bool tryMavlinkFirst;

private:
    void migrateTo(Handset *that) const;
    void startPPM() const;
    void startSerialProtocol() const;

    int input_detect = 0;
    RingbufHandle_t rb = nullptr;
    uint32_t lastDetect = 0;
};
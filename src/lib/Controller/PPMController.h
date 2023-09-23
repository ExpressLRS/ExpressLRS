#include "controller.h"

#include <driver/rmt.h>

class PPMController : public Controller
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
};
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
    unsigned long lastPPM = 0;
    RingbufHandle_t rb = nullptr;
};
#include "controller.h"

#include <driver/rmt.h>

class AutoDetect : public Controller
{
public:
    void Begin() override;
    void End() override;
    bool IsArmed() override;
    void handleInput() override;

private:
    void startPPM();
    void startCRSF();

    int input_detect = 0;
    RingbufHandle_t rb = nullptr;
};
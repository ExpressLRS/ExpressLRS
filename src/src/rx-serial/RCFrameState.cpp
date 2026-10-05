#include "RCFrameState.h"

#if defined(TARGET_RX) || defined(SRXL2_ADAPTER_TEST)
SerialRCFrameState serialRCFrames[2];

void ICACHE_RAM_ATTR crsfRCFrameAvailable()
{
    for (auto &frame : serialRCFrames)
    {
        frame.frameGeneration = frame.generation;
        frame.available = true;
    }
}

void ICACHE_RAM_ATTR crsfRCFrameMissed()
{
    for (auto &frame : serialRCFrames) frame.missed = true;
}

void ICACHE_RAM_ATTR crsfRCFrameReset()
{
    for (auto &frame : serialRCFrames) frame.invalidate();
}
#endif

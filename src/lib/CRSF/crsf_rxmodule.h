#pragma once

#include "targets.h"
#include "crsf_protocol.h"
#ifndef TARGET_NATIVE
#include "HardwareSerial.h"
#endif
#include "msp.h"
#include "msptypes.h"
#include "LowPassFilter.h"
#include "../CRC/crc.h"
#include "telemetry_protocol.h"
#include "TXModule.h"

#ifdef PLATFORM_ESP32
#include "esp32-hal-uart.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#endif

class CRSF_RXModule
{
public:
    CRSF_RXModule() {}

    void begin(TransportLayer* dev);

    void sendRCFrameToFC(Channels* chan);
    void sendLinkStatisticsToFC(Channels* chan);
    void sendMSPFrameToFC(uint8_t* data);

private:
    TransportLayer* _dev = nullptr;
};

extern CRSF_RXModule crsfRx;

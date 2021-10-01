#pragma once

#include <Arduino.h>

#define PIN_MOSI            13
#define PIN_CLK             14
#define PIN_CS              15

#define RF_API_DIR_GRTHAN   0x3E    // '>'
#define RF_API_DIR_EQUAL    0x3D    // '='
#define RF_API_BEEP_CMD     0x53    // 'S'
#define RF_API_CHANNEL_CMD  0x43    // 'C'
#define RF_API_BAND_CMD     0x42    // 'B'

class Rapidfire
{
public:
    void Init();
    void SendBuzzerCmd();
    void SendChannelCmd(uint8_t channel);
    void SendBandCmd(uint8_t band);

private:
    void SendSPI(uint8_t* buf, uint8_t bufLen);
    uint8_t crc8(uint8_t* buf, uint8_t bufLen);
};

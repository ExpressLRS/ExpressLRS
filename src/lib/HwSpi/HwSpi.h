#pragma once

#include <Arduino.h>
#include <SPI.h>

#define USE_HWSPI 1

class HwSpi : public SPIClass
{
public:
    HwSpi();

    void prepare(void)
    {
        pinMode(MOSI, OUTPUT);
        pinMode(MISO, INPUT);
        pinMode(SCK, OUTPUT);
        pinMode(SS, OUTPUT);
        digitalWrite(SS, HIGH);
        platform_init();
    }

    void set_ss(uint8_t state)
    {
        digitalWrite(SS, !!state);
    }

    void write(uint8_t data);
    void write(uint8_t *data, uint8_t numBytes);

private:
    int SS, MOSI, MISO, SCK;

    void platform_init(void);
};

extern HwSpi RadioSpi;

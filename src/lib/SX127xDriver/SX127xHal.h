#pragma once

#include <Arduino.h>
#include "../../src/targets.h"
#include "SPI.h"
#include "SX127x_Regs.h"

#ifndef ICACHE_RAM_ATTR //fix to allow both esp32 and esp8266
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif

class SX127xHal
{

public:
    void init(int MISO, int MOSI, int SCK, int NSS, int RST, int DIO0, int DIO1, int RXenb, int TXenb);

    void TXenable();
    void RXenable();

    uint8_t getRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);
    uint8_t readRegister(uint8_t reg);
    uint8_t readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes);

    uint8_t setRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0);
    void writeRegister(uint8_t reg, uint8_t data);
    void writeRegisterFIFO(uint8_t reg, const volatile uint8_t *data, uint8_t length);
    void writeRegisterBurst(uint8_t reg, uint8_t *data, uint8_t numBytes);

private:
    ////////Hardware Pins/////////////
    uint8_t GPIO_MOSI;
    uint8_t GPIO_MISO;
    uint8_t GPIO_SCK;
    uint8_t GPIO_RST;

    uint8_t GPIO_NSS;
    uint8_t GPIO_DIO0;
    uint8_t GPIO_DIO1;

    uint8_t GPIO_RXenb;
    uint8_t GPIO_TXenb;

    bool UsePApins = false; //uses seperate pins to toggle RX and TX (for external PA)
};
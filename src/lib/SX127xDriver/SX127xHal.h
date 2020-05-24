#pragma once

#include <Arduino.h>
#include <SPI.h>
#include "../../src/targets.h"
#include "SX127x_Regs.h"

typedef enum
{
    SX127x_INTERRUPT_NONE,
    SX127x_INTERRUPT_RX_DONE,
    SX127x_INTERRUPT_TX_DONE,
    SX127x_INTERRUPT_CAD
} SX127x_InterruptAssignment;

class SX127xHal
{

public:
    static SX127xHal *instance;

    SX127xHal(int MISO, int MOSI, int SCK, int NSS, int RST, int DIO0, int DIO1, int RXenb, int TXenb);

    void init();

    void ICACHE_RAM_ATTR TXenable();
    void ICACHE_RAM_ATTR RXenable();

    uint8_t ICACHE_RAM_ATTR getRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0);
    uint8_t ICACHE_RAM_ATTR readRegister(uint8_t reg);
    uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes);
    uint8_t ICACHE_RAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0);

    void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t data);
    void ICACHE_RAM_ATTR writeRegisterFIFO(uint8_t reg, const volatile uint8_t *data, uint8_t length);
    void ICACHE_RAM_ATTR writeRegisterBurst(uint8_t reg, uint8_t *data, uint8_t numBytes);

private:
    SX127x_InterruptAssignment InterruptAssignment = SX127x_INTERRUPT_NONE;
    ////////Hardware Pins/////////////
    int8_t GPIO_MOSI = -1;
    int8_t GPIO_MISO = -1;
    int8_t GPIO_SCK = -1;
    int8_t GPIO_RST = -1;

    int8_t GPIO_NSS = -1;
    int8_t GPIO_DIO0 = -1;
    int8_t GPIO_DIO1 = -1;

    int8_t GPIO_RXenb = -1;
    int8_t GPIO_TXenb = -1;

    bool UsePApins = false; //uses seperate pins to toggle RX and TX (for external PA)
};
#pragma once

#include "SX127xRegs.h"
#include "SX12xxDriverCommon.h"

class SX127xHal
{

public:
    static SX127xHal *instance;

    SX127xHal();

    void init();
    void end();
    void reset();

    void ICACHE_RAM_ATTR setNss(uint8_t radioNumber, bool state);

    uint8_t ICACHE_RAM_ATTR readRegisterBits(uint8_t reg, uint8_t mask, SX12XX_Radio_Number_t radioNumber);
    uint8_t ICACHE_RAM_ATTR readRegister(uint8_t reg, SX12XX_Radio_Number_t radioNumber);
    void ICACHE_RAM_ATTR readRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber);

    void ICACHE_RAM_ATTR writeRegisterBits(uint8_t reg, uint8_t value, uint8_t mask, SX12XX_Radio_Number_t radioNumber);
    void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t data, SX12XX_Radio_Number_t radioNumber);
    void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber);

    static ICACHE_RAM_ATTR void dioISR_1();
    static ICACHE_RAM_ATTR void dioISR_2();
    void (*IsrCallback_1)(); // function pointer for callback
    void (*IsrCallback_2)(); // function pointer for callback

private:

};
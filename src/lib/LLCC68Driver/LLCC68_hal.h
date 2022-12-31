#pragma once

/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2015 Semtech

Description: Handling of the node configuration protocol

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis and Gregory Cristian

Heavily modified/simplified by Alessandro Carcione 2020 for ELRS project
*/

#include "LLCC68_Regs.h"
#include "LLCC68.h"

enum LLCC68_BusyState_
{
    LLCC68_NOT_BUSY = true,
    LLCC68_BUSY = false,
};

class LLCC68Hal
{
public:
    static LLCC68Hal *instance;

    LLCC68Hal();

    void init();
    void end();
    void reset();

    void ICACHE_RAM_ATTR setNss(uint8_t radioNumber, bool state);

    void ICACHE_RAM_ATTR WriteCommand(LLCC68_RadioCommands_t command, uint8_t val, SX12XX_Radio_Number_t radioNumber, uint32_t busyDelay = 15);
    void ICACHE_RAM_ATTR WriteCommand(LLCC68_RadioCommands_t opcode, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber, uint32_t busyDelay = 15);
    void ICACHE_RAM_ATTR WriteRegister(uint16_t address, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber);
    void ICACHE_RAM_ATTR WriteRegister(uint16_t address, uint8_t value, SX12XX_Radio_Number_t radioNumber);

    void ICACHE_RAM_ATTR ReadCommand(LLCC68_RadioCommands_t opcode, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber);
    void ICACHE_RAM_ATTR ReadRegister(uint16_t address, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber);
    uint8_t ICACHE_RAM_ATTR ReadRegister(uint16_t address, SX12XX_Radio_Number_t radioNumber);

    void ICACHE_RAM_ATTR WriteBuffer(uint8_t offset, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber); // Writes and Reads to FIFO
    void ICACHE_RAM_ATTR ReadBuffer(uint8_t offset, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber);

    bool ICACHE_RAM_ATTR WaitOnBusy(SX12XX_Radio_Number_t radioNumber);

    void ICACHE_RAM_ATTR TXenable(SX12XX_Radio_Number_t radioNumber);
    void ICACHE_RAM_ATTR RXenable();
    void ICACHE_RAM_ATTR TXRXdisable();

    static ICACHE_RAM_ATTR void dioISR_1();
    static ICACHE_RAM_ATTR void dioISR_2();
    void (*IsrCallback_1)(); //function pointer for callback
    void (*IsrCallback_2)(); //function pointer for callback

    uint32_t BusyDelayStart;
    uint32_t BusyDelayDuration;
    void BusyDelay(uint32_t duration)
    {
        if (GPIO_PIN_BUSY == UNDEF_PIN)
        {
            BusyDelayStart = micros();
            BusyDelayDuration = duration;
        }
    }

private:
#if defined(PLATFORM_ESP32)
    uint64_t txrx_disable_clr_bits;
    uint64_t tx1_enable_set_bits;
    uint64_t tx1_enable_clr_bits;
    uint64_t tx2_enable_set_bits;
    uint64_t tx2_enable_clr_bits;
    uint64_t rx_enable_set_bits;
    uint64_t rx_enable_clr_bits;
#else
    bool rx_enabled;
    bool tx1_enabled;
    bool tx2_enabled;
#endif
};

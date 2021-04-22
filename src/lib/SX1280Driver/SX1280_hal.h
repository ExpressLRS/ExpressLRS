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

#include "SX1280_Regs.h"
#include "SX1280.h"

enum SX1280_InterruptAssignment_
{
    SX1280_INTERRUPT_NONE,
    SX1280_INTERRUPT_RX_DONE,
    SX1280_INTERRUPT_TX_DONE
};

enum SX1280_BusyState_
{
    SX1280_NOT_BUSY = true,
    SX1280_BUSY = false,
};

class SX1280Hal
{

private:
    volatile SX1280_InterruptAssignment_ InterruptAssignment = SX1280_INTERRUPT_NONE;

public:
    static SX1280Hal *instance;

    SX1280Hal();

    void init();
    void end();
    void reset();

    void ICACHE_RAM_ATTR WriteCommand(SX1280_RadioCommands_t opcode, uint8_t *buffer, uint8_t size);
    void ICACHE_RAM_ATTR WriteCommand(SX1280_RadioCommands_t command, uint8_t val);
    void ICACHE_RAM_ATTR WriteRegister(uint16_t address, uint8_t *buffer, uint8_t size);
    void ICACHE_RAM_ATTR WriteRegister(uint16_t address, uint8_t value);

    void ICACHE_RAM_ATTR ReadCommand(SX1280_RadioCommands_t opcode, uint8_t *buffer, uint8_t size);
    void ICACHE_RAM_ATTR ReadRegister(uint16_t address, uint8_t *buffer, uint8_t size);
    uint8_t ICACHE_RAM_ATTR ReadRegister(uint16_t address);

    void ICACHE_RAM_ATTR WriteBuffer(uint8_t offset, volatile uint8_t *buffer, uint8_t size); // Writes and Reads to FIFO
    void ICACHE_RAM_ATTR ReadBuffer(uint8_t offset, volatile uint8_t *buffer, uint8_t size);

    static void ICACHE_RAM_ATTR nullCallback(void);
    
    bool ICACHE_RAM_ATTR WaitOnBusy();
    static ICACHE_RAM_ATTR void dioISR();
    
    void ICACHE_RAM_ATTR TXenable();
    void ICACHE_RAM_ATTR RXenable();
    void ICACHE_RAM_ATTR TXRXdisable();

    static void (*TXdoneCallback)(); //function pointer for callback
    static void (*RXdoneCallback)(); //function pointer for callback

#if defined(GPIO_PIN_BUSY) && (GPIO_PIN_BUSY != UNDEF_PIN)
    void BusyDelay(uint32_t duration) const { (void)duration; };
#else
    uint32_t BusyDelayStart;
    uint32_t BusyDelayDuration;
    void BusyDelay(uint32_t duration)
    {
        BusyDelayStart = micros();
        BusyDelayDuration = duration;
    }
#endif
};

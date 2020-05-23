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

#include "LoRa_SX1280_Regs.h"
#include "LoRa_SX1280.h"
#include <SPI.h>

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
    const uint8_t SX1280_nss = GPIO_PIN_NSS;
    const uint8_t SX1280_busy = GPIO_PIN_BUSY;
    const uint8_t SX1280_dio1 = GPIO_PIN_DIO1;

    const uint8_t SX1280_MOSI = GPIO_PIN_MOSI;
    const uint8_t SX1280_MISO = GPIO_PIN_MISO;
    const uint8_t SX1280_SCK = GPIO_PIN_SCK;
    const uint8_t SX1280_RST = GPIO_PIN_RST;

    const uint8_t SX1280_RXenb = GPIO_PIN_RX_ENABLE;
    const uint8_t SX1280_TXenb = GPIO_PIN_RX_ENABLE;

    SX1280_InterruptAssignment_ InterruptAssignment = SX1280_INTERRUPT_NONE;
    SX1280_BusyState_ BusyState = SX1280_NOT_BUSY;

public:
    static SX1280Hal *instance;

    SX1280Hal();

    

    void init();
    void SetSpiSpeed(uint32_t spiSpeed);
    void reset();

    void ICACHE_RAM_ATTR WriteCommand(SX1280_RadioCommands_t opcode, uint8_t *buffer, uint16_t size);
    void ICACHE_RAM_ATTR WriteCommand(SX1280_RadioCommands_t command, uint8_t val);
    void ICACHE_RAM_ATTR WriteRegister(uint16_t address, uint8_t *buffer, uint16_t size);
    void ICACHE_RAM_ATTR WriteRegister(uint16_t address, uint8_t value);

    void ICACHE_RAM_ATTR ReadCommand(SX1280_RadioCommands_t opcode, uint8_t *buffer, uint16_t size);
    void ICACHE_RAM_ATTR ReadRegister(uint16_t address, uint8_t *buffer, uint16_t size);
    uint8_t ICACHE_RAM_ATTR ReadRegister(uint16_t address);

    void ICACHE_RAM_ATTR WriteBuffer(uint8_t offset, uint8_t *buffer, uint8_t size); // Writes and Reads to FIFO
    void ICACHE_RAM_ATTR ReadBuffer(uint8_t offset, uint8_t *buffer, uint8_t size);

    void ICACHE_RAM_ATTR setIRQassignment(SX1280_InterruptAssignment_ newInterruptAssignment);

    static void ICACHE_RAM_ATTR nullCallback(void);
    
    void ICACHE_RAM_ATTR WaitOnBusy();
    static ICACHE_RAM_ATTR void dioISR();
    static ICACHE_RAM_ATTR void busyISR();
    static void (*TXdoneCallback)(); //function pointer for callback
    static void (*RXdoneCallback)(); //function pointer for callback
};

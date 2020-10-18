/*
  ______                              _
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2016 Semtech

Description: Handling of the node configuration protocol

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Gregory Cristian and Matthieu Verdy

Modified and adapted by Alessandro Carcione for ELRS project 
*/

#include "SX1280_Regs.h"
#include "SX1280_hal.h"
#include <SPI.h>

SX1280Hal *SX1280Hal::instance = NULL;

void ICACHE_RAM_ATTR SX1280Hal::nullCallback(void) {}

void (*SX1280Hal::TXdoneCallback)() = &nullCallback;
void (*SX1280Hal::RXdoneCallback)() = &nullCallback;

SX1280Hal::SX1280Hal()
{
    instance = this;
}

void SX1280Hal::end()
{
    SPI.end();
    detachInterrupt(GPIO_PIN_DIO1);
}

void SX1280Hal::init()
{
    Serial.println("Hal Init");
    pinMode(GPIO_PIN_BUSY, INPUT);
    pinMode(GPIO_PIN_DIO1, INPUT);

    pinMode(GPIO_PIN_RST, OUTPUT);
    pinMode(GPIO_PIN_NSS, OUTPUT);

#if defined(GPIO_PIN_RX_ENABLE) || defined(GPIO_PIN_TX_ENABLE)
    Serial.print("This Target uses seperate TX/RX enable pins: ");
#endif

#if defined(GPIO_PIN_TX_ENABLE)
    Serial.print("TX: ");
    Serial.print(GPIO_PIN_TX_ENABLE);
#endif

#if defined(GPIO_PIN_RX_ENABLE)
    Serial.print(" RX: ");
    Serial.println(GPIO_PIN_RX_ENABLE);
#endif

#if defined(GPIO_PIN_RX_ENABLE)
    pinMode(GPIO_PIN_RX_ENABLE, OUTPUT);
    digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
#endif

#if defined(GPIO_PIN_TX_ENABLE)
    pinMode(GPIO_PIN_TX_ENABLE, OUTPUT);
    digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
#endif

#ifdef PLATFORM_ESP32
    SPI.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI, -1); // sck, miso, mosi, ss (ss can be any GPIO)
    gpio_pullup_en((gpio_num_t)GPIO_PIN_MISO);
    SPI.setFrequency(10000000);
#endif

#ifdef PLATFORM_ESP8266
    Serial.println("PLATFORM_ESP8266");
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(10000000);
#endif

#ifdef PLATFORM_STM32
    SPI.setMOSI(GPIO_PIN_MOSI);
    SPI.setMISO(GPIO_PIN_MISO);
    SPI.setSCLK(GPIO_PIN_SCK);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
#endif

    //attachInterrupt(digitalPinToInterrupt(GPIO_PIN_BUSY), this->busyISR, CHANGE); //not used atm
    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO1), this->dioISR, RISING);
}

void SX1280Hal::reset(void)
{
    Serial.println("SX1280 Reset");
    delay(50);
    digitalWrite(GPIO_PIN_RST, LOW);
    delay(50);
    digitalWrite(GPIO_PIN_RST, HIGH);

    while (digitalRead(GPIO_PIN_BUSY) == HIGH) // wait for busy
    {
        #ifdef PLATFORM_STM32
        __NOP();
        #elif PLATFORM_ESP32
        _NOP();
        #elif PLATFORM_ESP8266
        _NOP();
        #endif
    }

    //this->BusyState = SX1280_NOT_BUSY;
    Serial.println("SX1280 Ready!");
}

void ICACHE_RAM_ATTR SX1280Hal::WriteCommand(SX1280_RadioCommands_t command, uint8_t val)
{
    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);

    SPI.transfer((uint8_t)command);
    SPI.transfer(val);

    digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX1280Hal::WriteCommand(SX1280_RadioCommands_t command, uint8_t *buffer, uint8_t size)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[size + 1];

    OutBuffer[0] = (uint8_t)command;
    memcpy(OutBuffer + 1, buffer, size);

    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);
    SPI.transfer(OutBuffer, (uint8_t)sizeof(OutBuffer));
    digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX1280Hal::ReadCommand(SX1280_RadioCommands_t command, uint8_t *buffer, uint8_t size)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[size + 2];
    #define RADIO_GET_STATUS_BUF_SIZEOF 3 // special case for command == SX1280_RADIO_GET_STATUS, fixed 3 bytes packet size

    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);

    if (command == SX1280_RADIO_GET_STATUS)
    {
        OutBuffer[0] = (uint8_t)command;
        OutBuffer[1] = 0x00;
        OutBuffer[2] = 0x00;
        SPI.transfer(OutBuffer, RADIO_GET_STATUS_BUF_SIZEOF);
        buffer[0] = OutBuffer[0];
    }
    else
    {
        OutBuffer[0] = (uint8_t)command;
        OutBuffer[1] = 0x00;
        memcpy(OutBuffer + 2, buffer, size);
        SPI.transfer(OutBuffer, sizeof(OutBuffer));
        memcpy(buffer, OutBuffer + 2, size);
    }
    digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX1280Hal::WriteRegister(uint16_t address, uint8_t *buffer, uint8_t size)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[size + 3];

    OutBuffer[0] = (SX1280_RADIO_WRITE_REGISTER);
    OutBuffer[1] = ((address & 0xFF00) >> 8);
    OutBuffer[2] = (address & 0x00FF);

    memcpy(OutBuffer + 3, buffer, size);

    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);
    SPI.transfer(OutBuffer, (uint8_t)sizeof(OutBuffer));
    digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX1280Hal::WriteRegister(uint16_t address, uint8_t value)
{
    WriteRegister(address, &value, 1);
}

void ICACHE_RAM_ATTR SX1280Hal::ReadRegister(uint16_t address, uint8_t *buffer, uint8_t size)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[size + 4];

    OutBuffer[0] = (SX1280_RADIO_READ_REGISTER);
    OutBuffer[1] = ((address & 0xFF00) >> 8);
    OutBuffer[2] = (address & 0x00FF);
    OutBuffer[3] = 0x00;

    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);

    memcpy(OutBuffer + 4, buffer, size);
    SPI.transfer(OutBuffer, uint8_t(sizeof(OutBuffer)));
    memcpy(buffer, OutBuffer + 4, size);

    digitalWrite(GPIO_PIN_NSS, HIGH);
}

uint8_t ICACHE_RAM_ATTR SX1280Hal::ReadRegister(uint16_t address)
{
    uint8_t data;
    ReadRegister(address, &data, 1);
    return data;
}

void ICACHE_RAM_ATTR SX1280Hal::WriteBuffer(uint8_t offset, volatile uint8_t *buffer, uint8_t size)
{
    uint8_t localbuf[size];

    for (int i = 0; i < size; i++) // todo check if this is the right want to handle volatiles
    {
        localbuf[i] = buffer[i];
    }

    WORD_ALIGNED_ATTR uint8_t OutBuffer[size + 2];

    OutBuffer[0] = SX1280_RADIO_WRITE_BUFFER;
    OutBuffer[1] = offset;

    memcpy(OutBuffer + 2, localbuf, size);

    WaitOnBusy();

    digitalWrite(GPIO_PIN_NSS, LOW);
    SPI.transfer(OutBuffer, (uint8_t)sizeof(OutBuffer));
    digitalWrite(GPIO_PIN_NSS, HIGH);
}

void ICACHE_RAM_ATTR SX1280Hal::ReadBuffer(uint8_t offset, volatile uint8_t *buffer, uint8_t size)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[size + 3];
    uint8_t localbuf[size];

    OutBuffer[0] = SX1280_RADIO_READ_BUFFER;
    OutBuffer[1] = offset;
    OutBuffer[2] = 0x00;
    memcpy(OutBuffer + 3, localbuf, size);

    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);

    SPI.transfer(OutBuffer, uint8_t(sizeof(OutBuffer)));
    digitalWrite(GPIO_PIN_NSS, HIGH);

    memcpy(localbuf, OutBuffer + 3, size);

    for (int i = 0; i < size; i++) // todo check if this is the right wany to handle volatiles
    {
        buffer[i] = localbuf[i];
    }
}

void ICACHE_RAM_ATTR SX1280Hal::WaitOnBusy()
{
    #define wtimeoutUS 5000
    uint32_t startTime = micros();

    while (digitalRead(GPIO_PIN_BUSY) == HIGH) // wait untill not busy or until wtimeoutUS
    {
        if (micros() > startTime + wtimeoutUS)
        {
            return;
        }
        else
        {
            #ifdef PLATFORM_STM32
            __NOP();
            #elif PLATFORM_ESP32
            _NOP();
            #elif PLATFORM_ESP8266
            _NOP();
            #endif
        }
    }
}

void ICACHE_RAM_ATTR SX1280Hal::dioISR()
{
    if (instance->InterruptAssignment == SX1280_INTERRUPT_RX_DONE)
    {
        RXdoneCallback();
    }
    else if (instance->InterruptAssignment == SX1280_INTERRUPT_TX_DONE)
    {
        TXdoneCallback();
    }
}

void ICACHE_RAM_ATTR SX1280Hal::TXenable()
{
    if (instance->InterruptAssignment != SX1280_INTERRUPT_TX_DONE)
    {
        instance->InterruptAssignment = SX1280_INTERRUPT_TX_DONE;
        //Serial.println("TXenb");
    }

    #if defined(GPIO_PIN_RX_ENABLE)
    digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
    #endif

    #if defined(GPIO_PIN_TX_ENABLE)
    digitalWrite(GPIO_PIN_TX_ENABLE, HIGH);
    #endif
}

void ICACHE_RAM_ATTR SX1280Hal::RXenable()
{

    if (instance->InterruptAssignment != SX1280_INTERRUPT_RX_DONE)
    {
        instance->InterruptAssignment = SX1280_INTERRUPT_RX_DONE;
        //Serial.println("RXenb");
    }

    #if defined(GPIO_PIN_RX_ENABLE)
    digitalWrite(GPIO_PIN_RX_ENABLE, HIGH);
    #endif

    #if defined(GPIO_PIN_TX_ENABLE)
    digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
    #endif
}

void ICACHE_RAM_ATTR SX1280Hal::TXRXdisable()
{
    if (this->InterruptAssignment != SX1280_INTERRUPT_NONE)
    {
        this->InterruptAssignment = SX1280_INTERRUPT_NONE;
    }
    #if defined(GPIO_PIN_RX_ENABLE)
    digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
    #endif

    #if defined(GPIO_PIN_TX_ENABLE)
    digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
    #endif
}
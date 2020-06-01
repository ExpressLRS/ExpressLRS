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
*/
#include "LoRa_SX1280_Regs.h"
#include "LoRa_SX1280_hal.h"
#include <SPI.h>

SX1280Hal *SX1280Hal::instance = NULL;

void ICACHE_RAM_ATTR SX1280Hal::nullCallback(void){};

void (*SX1280Hal::TXdoneCallback)() = &nullCallback;
void (*SX1280Hal::RXdoneCallback)() = &nullCallback;

SX1280Hal::SX1280Hal()
{
    instance = this;
}

void SX1280Hal::init()
{
    Serial.println("Spi Begin");
    pinMode(GPIO_PIN_BUSY, INPUT);
    pinMode(GPIO_PIN_DIO1, INPUT);

    pinMode(GPIO_PIN_RST, OUTPUT);
    pinMode(GPIO_PIN_NSS, OUTPUT);

#ifdef PLATFORM_ESP32
    SPI.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI, -1); // sck, miso, mosi, ss (ss can be any GPIO)
    SPI.setFrequency(8000000);
#endif

#ifdef PLATFORM_ESP8266
    Serial.println("PLATFORM_ESP8266");
    SPI.begin();
    //SPI.pins(this->SX1280_SCK, this->SX1280_MISO, this->SX1280_MOSI, -1);
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

    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_BUSY), this->busyISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO1), this->dioISR, RISING);
}

void ICACHE_RAM_ATTR SX1280Hal::reset(void)
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

    this->BusyState = SX1280_NOT_BUSY;
    Serial.println("SX1280 Ready!");
}

void ICACHE_RAM_ATTR SX1280Hal::WriteCommand(SX1280_RadioCommands_t command, uint8_t val)
{
    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);

#ifdef PLATFORM_STM32
    SPI.transfer((uint8_t)command);
    SPI.transfer(val);
#else
    SPI.write((uint8_t)command);
    SPI.write(val);
#endif
    digitalWrite(GPIO_PIN_NSS, HIGH);

    if (command != SX1280_RADIO_SET_SLEEP)
    {
        WaitOnBusy();
    }
}

void ICACHE_RAM_ATTR SX1280Hal::WriteCommand(SX1280_RadioCommands_t command, uint8_t *buffer, uint16_t size)
{
    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);

#ifdef PLATFORM_STM32
    SPI.transfer((uint8_t)command);
#else
    SPI.write((uint8_t)command);
#endif
    for (uint16_t i = 0; i < size; i++)
    {
#ifdef PLATFORM_STM32
        SPI.transfer(buffer[i]);
#else
        SPI.write(buffer[i]);
#endif
    }
    digitalWrite(GPIO_PIN_NSS, HIGH);

    if (command != SX1280_RADIO_SET_SLEEP)
    {
        WaitOnBusy();
    }
}

void ICACHE_RAM_ATTR SX1280Hal::ReadCommand(SX1280_RadioCommands_t command, uint8_t *buffer, uint16_t size)
{

    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);

    if (command == SX1280_RADIO_GET_STATUS)
    {
        buffer[0] = SPI.transfer((uint8_t)command);
#ifdef PLATFORM_STM32
        SPI.transfer(0);
        SPI.transfer(0);
#else
        SPI.write(0);
        SPI.write(0);
#endif
    }
    else
    {
#ifdef PLATFORM_STM32
        SPI.transfer((uint8_t)command);
        SPI.transfer(0);
#else
        SPI.write((uint8_t)command);
        SPI.write(0);
#endif
        for (uint16_t i = 0; i < size; i++)
        {
            buffer[i] = SPI.transfer(0);
        }
    }
    digitalWrite(GPIO_PIN_NSS, HIGH);

    WaitOnBusy();
}

void ICACHE_RAM_ATTR SX1280Hal::WriteRegister(uint16_t address, uint8_t *buffer, uint16_t size)
{

    WaitOnBusy();
    digitalWrite(GPIO_PIN_NSS, LOW);

#ifdef PLATFORM_STM32
    SPI.transfer(SX1280_RADIO_WRITE_REGISTER);
    SPI.transfer((address & 0xFF00) >> 8);
    SPI.transfer(address & 0x00FF);
#else
    SPI.write(SX1280_RADIO_WRITE_REGISTER);
    SPI.write((address & 0xFF00) >> 8);
    SPI.write(address & 0x00FF);
#endif
    for (uint16_t i = 0; i < size; i++)
    {
#ifdef PLATFORM_STM32
        SPI.transfer(buffer[i]);
#else
        SPI.write(buffer[i]);
#endif
    }
    digitalWrite(GPIO_PIN_NSS, HIGH);

    WaitOnBusy();
}

void ICACHE_RAM_ATTR SX1280Hal::WriteRegister(uint16_t address, uint8_t value)
{
    WriteRegister(address, &value, 1);
}

void ICACHE_RAM_ATTR SX1280Hal::ReadRegister(uint16_t address, uint8_t *buffer, uint16_t size)
{
    WaitOnBusy();

    digitalWrite(GPIO_PIN_NSS, LOW);
#ifdef PLATFORM_STM32
    SPI.transfer(SX1280_RADIO_READ_REGISTER);
    SPI.transfer((address & 0xFF00) >> 8);
    SPI.transfer(address & 0x00FF);
    SPI.transfer(0);
#else
    SPI.write(SX1280_RADIO_READ_REGISTER);
    SPI.write((address & 0xFF00) >> 8);
    SPI.write(address & 0x00FF);
    SPI.write(0);
#endif
    for (uint16_t i = 0; i < size; i++)
    {
        buffer[i] = SPI.transfer(0);
    }
    digitalWrite(GPIO_PIN_NSS, HIGH);

    WaitOnBusy();
}

uint8_t ICACHE_RAM_ATTR SX1280Hal::ReadRegister(uint16_t address)
{
    uint8_t data;
    ReadRegister(address, &data, 1);
    return data;
}

void ICACHE_RAM_ATTR SX1280Hal::WriteBuffer(uint8_t offset, uint8_t *buffer, uint8_t size)
{
    WaitOnBusy();

    digitalWrite(GPIO_PIN_NSS, LOW);
#ifdef PLATFORM_STM32
    SPI.transfer(SX1280_RADIO_WRITE_BUFFER);
    SPI.transfer(offset);
#else
    SPI.write(SX1280_RADIO_WRITE_BUFFER);
    SPI.write(offset);
#endif
    for (uint16_t i = 0; i < size; i++)
    {
#ifdef PLATFORM_STM32
        SPI.transfer(buffer[i]);
#else
        SPI.write(buffer[i]);
#endif
    }
    digitalWrite(GPIO_PIN_NSS, HIGH);

    WaitOnBusy();
}

void ICACHE_RAM_ATTR SX1280Hal::ReadBuffer(uint8_t offset, uint8_t *buffer, uint8_t size)
{
    WaitOnBusy();

    digitalWrite(GPIO_PIN_NSS, LOW);
#ifdef PLATFORM_STM32
    SPI.transfer(SX1280_RADIO_READ_BUFFER);
    SPI.transfer(offset);
    SPI.transfer(0);
#else
    SPI.write(SX1280_RADIO_READ_BUFFER);
    SPI.write(offset);
    SPI.write(0);
#endif
    for (uint16_t i = 0; i < size; i++)
    {
        buffer[i] = SPI.transfer(0);
    }
    digitalWrite(GPIO_PIN_NSS, HIGH);

    WaitOnBusy();
}

void ICACHE_RAM_ATTR SX1280Hal::WaitOnBusy()
{
    //while (instance->BusyState != SX1280_NOT_BUSY)
    while (digitalRead(GPIO_PIN_BUSY) == HIGH)
    {
        //if (instance->BusyState == SX1280_BUSY)
        // {
        //     Serial.println("wB");
        // }
        // else
        // {
        //     Serial.println("wnB");
        // }
        //{
        //Serial.println("W");
#ifdef PLATFORM_ESP32
        NOP();
#endif
#ifdef PLATFORM_ESP8266
        _NOP();
        //delayMicroseconds(1);
#endif
    }
}

void ICACHE_RAM_ATTR SX1280Hal::busyISR()
{
    if (digitalRead(GPIO_PIN_BUSY) == HIGH)
    {
        instance->BusyState = SX1280_BUSY;
        //Serial.println("B");
    }
    else
    {
        instance->BusyState = SX1280_NOT_BUSY;
        //Serial.println("Bn");
    }
}

void ICACHE_RAM_ATTR SX1280Hal::dioISR()
{
    Serial.println("DIOISR");
    if (instance->InterruptAssignment == SX1280_INTERRUPT_RX_DONE)
    {
        //Serial.println("HalRXdone");
        RXdoneCallback();
    }
    else if (instance->InterruptAssignment == SX1280_INTERRUPT_TX_DONE)
    {
        //Serial.println("HalTXdone");
        TXdoneCallback();
    }
}

void ICACHE_RAM_ATTR SX1280Hal::setIRQassignment(SX1280_InterruptAssignment_ newInterruptAssignment)
{

    // if (InterruptAssignment == newInterruptAssignment)
    // {
    //     return;
    // }
    // else
    // {
    if (newInterruptAssignment == SX1280_INTERRUPT_TX_DONE)
    {
        this->InterruptAssignment = SX1280_INTERRUPT_TX_DONE;
    }
    else if (newInterruptAssignment == SX1280_INTERRUPT_RX_DONE)
    {
        this->InterruptAssignment = SX1280_INTERRUPT_RX_DONE;
    }
    //}
}

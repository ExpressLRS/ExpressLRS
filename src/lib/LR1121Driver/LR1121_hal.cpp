#ifndef UNIT_TEST

#include "LR1121_hal.h"
#include "LR1121_Regs.h"
#include "logging.h"
#include <SPI.h>

LR1121Hal *LR1121Hal::instance = NULL;

LR1121Hal::LR1121Hal()
{
    instance = this;
}

void LR1121Hal::end()
{
    detachInterrupt(GPIO_PIN_DIO1);
    if (GPIO_PIN_DIO1_2 != UNDEF_PIN)
    {
        detachInterrupt(GPIO_PIN_DIO1_2);
    }
    SPI.end();
    IsrCallback_1 = nullptr; // remove callbacks
    IsrCallback_2 = nullptr; // remove callbacks
}

void LR1121Hal::init()
{
    DBGLN("Hal Init");

    pinMode(GPIO_PIN_BUSY, INPUT);
    if (GPIO_PIN_BUSY_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_BUSY_2, INPUT);
    }

    pinMode(GPIO_PIN_DIO1, INPUT);
    if (GPIO_PIN_DIO1_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_DIO1_2, INPUT);
    }

    pinMode(GPIO_PIN_NSS, OUTPUT);
    digitalWrite(GPIO_PIN_NSS, HIGH);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_NSS_2, OUTPUT);
        digitalWrite(GPIO_PIN_NSS_2, HIGH);
    }

#ifdef PLATFORM_ESP32
    SPI.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI, GPIO_PIN_NSS); // sck, miso, mosi, ss (ss can be any GPIO)
    gpio_pullup_en((gpio_num_t)GPIO_PIN_MISO);
    SPI.setFrequency(10000000);
    SPI.setHwCs(true);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        spiAttachSS(SPI.bus(), 1, GPIO_PIN_NSS_2);
    }
    spiEnableSSPins(SPI.bus(), SX12XX_Radio_All);
#elif defined(PLATFORM_ESP8266)
    DBGLN("PLATFORM_ESP8266");
    SPI.begin();
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.setFrequency(10000000);
#elif defined(PLATFORM_STM32)
    DBGLN("Config SPI");
    SPI.setMOSI(GPIO_PIN_MOSI);
    SPI.setMISO(GPIO_PIN_MISO);
    SPI.setSCLK(GPIO_PIN_SCK);
    SPI.setBitOrder(MSBFIRST);
    SPI.setDataMode(SPI_MODE0);
    SPI.begin();
    SPI.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
#endif

    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO1), this->dioISR_1, RISING);
    if (GPIO_PIN_DIO1_2 != UNDEF_PIN)
    {
        attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO1_2), this->dioISR_2, RISING);
    }
}

void ICACHE_RAM_ATTR LR1121Hal::setNss(uint8_t radioNumber, bool state)
{
    if (state == LOW)
        WaitOnBusy(radioNumber);

#if defined(PLATFORM_ESP32)
    // if (state == HIGH)
    //     return;

    spiDisableSSPins(SPI.bus(), ~radioNumber);
    spiEnableSSPins(SPI.bus(), radioNumber);
#else
    if (radioNumber & SX12XX_Radio_1)
        digitalWrite(GPIO_PIN_NSS, state);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN && radioNumber & SX12XX_Radio_2)
        digitalWrite(GPIO_PIN_NSS_2, state);
#endif
}

void LR1121Hal::reset(void)
{
    DBGLN("LR1121 Reset");

    if (GPIO_PIN_RST != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_RST, OUTPUT);
        digitalWrite(GPIO_PIN_RST, LOW);
        delay(1);
        digitalWrite(GPIO_PIN_RST, HIGH);
        delay(300); // LR1121 busy is high for 230ms after reset.  The WaitOnBusy timeout is only 1ms.  So this long delay is required.
    }

    WaitOnBusy(SX12XX_Radio_All);
}

void ICACHE_RAM_ATTR LR1121Hal::WriteCommand(uint16_t command, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[size + 2];

    OutBuffer[0] = ((command & 0xFF00) >> 8);
    OutBuffer[1] = (command & 0x00FF);
    memcpy(OutBuffer + 2, buffer, size);

    setNss(radioNumber, LOW);
    SPI.transfer(OutBuffer, (uint8_t)sizeof(OutBuffer));
    setNss(radioNumber, HIGH);
    
    memcpy(buffer, OutBuffer+2, size);
}

void ICACHE_RAM_ATTR LR1121Hal::WriteCommand(uint16_t command, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[2];

    OutBuffer[0] = ((command & 0xFF00) >> 8);
    OutBuffer[1] = (command & 0x00FF);

    setNss(radioNumber, LOW);
    SPI.transfer(OutBuffer, (uint8_t)sizeof(OutBuffer));
    setNss(radioNumber, HIGH);
}

void ICACHE_RAM_ATTR LR1121Hal::ReadCommand(uint16_t command, uint8_t *argBuffer, uint8_t argSize, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t CmdBuffer[2 + argSize];

    CmdBuffer[0] = ((command & 0xFF00) >> 8);
    CmdBuffer[1] = (command & 0x00FF);

    memcpy(CmdBuffer + 2, argBuffer, argSize);

    setNss(radioNumber, LOW);
    SPI.transfer(CmdBuffer, 2 + argSize);
    setNss(radioNumber, HIGH);

    WORD_ALIGNED_ATTR uint8_t InBuffer[size];

    setNss(radioNumber, LOW);
    SPI.transfer(InBuffer, size);
    setNss(radioNumber, HIGH);

    memcpy(buffer, InBuffer, size);
}

void ICACHE_RAM_ATTR LR1121Hal::ReadCommand(uint16_t command, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t CmdBuffer[2];

    CmdBuffer[0] = ((command & 0xFF00) >> 8);
    CmdBuffer[1] = (command & 0x00FF);

    setNss(radioNumber, LOW);
    SPI.transfer(CmdBuffer, 2);
    setNss(radioNumber, HIGH);

    WORD_ALIGNED_ATTR uint8_t InBuffer[size];

    setNss(radioNumber, LOW);
    SPI.transfer(InBuffer, size);
    setNss(radioNumber, HIGH);

    memcpy(buffer, InBuffer, size);
}

bool ICACHE_RAM_ATTR LR1121Hal::WaitOnBusy(SX12XX_Radio_Number_t radioNumber)
{
    // if (GPIO_PIN_BUSY != UNDEF_PIN)
    // {
        constexpr uint32_t wtimeoutUS = 1000U;
        uint32_t startTime = 0;

        while (true)
        {
            if (radioNumber == SX12XX_Radio_1)
            {
                if (digitalRead(GPIO_PIN_BUSY) == LOW) return true;
            }
            else if (GPIO_PIN_BUSY_2 != UNDEF_PIN && radioNumber == SX12XX_Radio_2)
            {
                if (digitalRead(GPIO_PIN_BUSY_2) == LOW) return true;
            }
            else if (radioNumber == SX12XX_Radio_All)
            {
                if (GPIO_PIN_BUSY_2 != UNDEF_PIN)
                {
                    if (digitalRead(GPIO_PIN_BUSY) == LOW && digitalRead(GPIO_PIN_BUSY_2) == LOW) return true;
                }
                else
                {
                    if (digitalRead(GPIO_PIN_BUSY) == LOW) return true;
                }
            }
            // Use this time to call micros().
            uint32_t now = micros();
            if (startTime == 0) startTime = now;
            if ((now - startTime) > wtimeoutUS) return false;
        }
    // }
    // else
    // {
        // uint32_t now = micros();
        // while ((now - BusyDelayStart) < BusyDelayDuration)
        //     now = micros();
        // BusyDelayDuration = 0;
    // }
    // return true;
}

void ICACHE_RAM_ATTR LR1121Hal::dioISR_1()
{
    if (instance->IsrCallback_1)
        instance->IsrCallback_1();
}

void ICACHE_RAM_ATTR LR1121Hal::dioISR_2()
{
    if (instance->IsrCallback_2)
        instance->IsrCallback_2();
}

#endif // UNIT_TEST

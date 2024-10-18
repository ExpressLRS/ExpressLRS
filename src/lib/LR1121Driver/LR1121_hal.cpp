#ifndef UNIT_TEST

#include "LR1121_hal.h"
#include "LR1121_Regs.h"
#include "logging.h"
#include <SPIEx.h>

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
    SPIEx.end();
    IsrCallback_1 = nullptr;
    IsrCallback_2 = nullptr;
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
    SPIEx.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI, GPIO_PIN_NSS); // sck, miso, mosi, ss (ss can be any GPIO)
    gpio_pullup_en((gpio_num_t)GPIO_PIN_MISO);
    SPIEx.setFrequency(17500000);
    SPIEx.setHwCs(true);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_NSS_2, OUTPUT);
        digitalWrite(GPIO_PIN_NSS_2, HIGH);
        spiAttachSS(SPIEx.bus(), 1, GPIO_PIN_NSS_2);
    }
    spiEnableSSPins(SPIEx.bus(), SX12XX_Radio_All);
#elif defined(PLATFORM_ESP8266)
    DBGLN("PLATFORM_ESP8266");
    SPIEx.begin();
    SPIEx.setHwCs(true);
    SPIEx.setBitOrder(MSBFIRST);
    SPIEx.setDataMode(SPI_MODE0);
    SPIEx.setFrequency(17500000);
#endif

    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO1), this->dioISR_1, RISING);
    if (GPIO_PIN_DIO1_2 != UNDEF_PIN)
    {
        attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO1_2), this->dioISR_2, RISING);
    }
}

void LR1121Hal::reset(bool bootloader)
{
    DBGLN("LR1121 Reset");

    if (GPIO_PIN_RST != UNDEF_PIN)
    {
        if (bootloader)
        {
            pinMode(GPIO_PIN_BUSY, OUTPUT);
            digitalWrite(GPIO_PIN_BUSY, LOW);
        }
        pinMode(GPIO_PIN_RST, OUTPUT);
        digitalWrite(GPIO_PIN_RST, LOW);
        if (GPIO_PIN_RST_2 != UNDEF_PIN)
        {
            if (bootloader)
            {
                pinMode(GPIO_PIN_BUSY_2, OUTPUT);
                digitalWrite(GPIO_PIN_BUSY_2, LOW);
            }
            pinMode(GPIO_PIN_RST_2, OUTPUT);
            digitalWrite(GPIO_PIN_RST_2, LOW);
        }
        delay(1);
        digitalWrite(GPIO_PIN_RST, HIGH);
        if (GPIO_PIN_RST_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_RST_2, HIGH);
        }
        delay(300); // LR1121 busy is high for 230ms after reset.  The WaitOnBusy timeout is only 1ms.  So this long delay is required.
        if (bootloader)
        {
            pinMode(GPIO_PIN_BUSY, INPUT);
            if (GPIO_PIN_RST_2 != UNDEF_PIN)
            {
                pinMode(GPIO_PIN_BUSY_2, INPUT);
            }
            delay(100);
        }
    }

    WaitOnBusy(SX12XX_Radio_All);
}

void ICACHE_RAM_ATTR LR1121Hal::WriteCommand(uint16_t command, uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[WORD_PADDED(size + 2)] = {
        (uint8_t)((command & 0xFF00) >> 8),
        (uint8_t)(command & 0x00FF),
    };

    memcpy(OutBuffer + 2, buffer, size);

    WaitOnBusy(radioNumber);
    SPIEx.write(radioNumber, OutBuffer, size + 2);
}

void ICACHE_RAM_ATTR LR1121Hal::WriteCommand(uint16_t command, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[WORD_PADDED(2)] = {
        (uint8_t)((command & 0xFF00) >> 8),
        (uint8_t)(command & 0x00FF)
    };

    WaitOnBusy(radioNumber);
    SPIEx.write(radioNumber, OutBuffer, 2);
}

void ICACHE_RAM_ATTR LR1121Hal::ReadCommand(uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t InBuffer[WORD_PADDED(size)] = {0};

    memcpy(InBuffer, buffer, size);

    WaitOnBusy(radioNumber);
    SPIEx.read(radioNumber, InBuffer, size);

    memcpy(buffer, InBuffer, size);
}

bool ICACHE_RAM_ATTR LR1121Hal::WaitOnBusy(SX12XX_Radio_Number_t radioNumber)
{
    constexpr uint32_t wtimeoutUS = 1000U;
    uint32_t startTime = 0;

    while (true)
    {
        if (radioNumber == SX12XX_Radio_1)
        {
            if (digitalRead(GPIO_PIN_BUSY) == LOW) return true;
        }
        else if (radioNumber == SX12XX_Radio_2)
        {
            if (GPIO_PIN_BUSY_2 == UNDEF_PIN || digitalRead(GPIO_PIN_BUSY_2) == LOW) return true;
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

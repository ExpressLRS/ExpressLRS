#ifndef UNIT_TEST

#include "LR2021_hal.h"
#include "logging.h"
#include <SPIEx.h>

LR2021Hal *LR2021Hal::instance = nullptr;

LR2021Hal::LR2021Hal()
{
    instance = this;
}

void LR2021Hal::end()
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

void LR2021Hal::init()
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
    SPIEx.setFrequency(16000000);
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
    SPIEx.setFrequency(16000000);
#endif

    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO1), dioISR_1, RISING);
    if (GPIO_PIN_DIO1_2 != UNDEF_PIN)
    {
        attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO1_2), dioISR_2, RISING);
    }
}

void LR2021Hal::reset(const bool bootloader)
{
    DBGLN("LR2021 Reset");

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
        delay(300); // LR2021 busy is high for 230ms after reset.  The WaitOnBusy timeout is only 1ms.  So this long delay is required.
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

uint16_t ICACHE_RAM_ATTR LR2021Hal::WriteCommand(uint16_t command, const uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[WORD_PADDED(size + 2)] = {
        (uint8_t)((command & 0xFF00) >> 8),
        (uint8_t)(command & 0x00FF),
    };

    memcpy(OutBuffer + 2, buffer, size);

    WaitOnBusy(radioNumber);
    SPIEx.read(radioNumber, OutBuffer, size + 2);
    return OutBuffer[0] << 8 | OutBuffer[1];
}

uint16_t ICACHE_RAM_ATTR LR2021Hal::WriteCommand(uint16_t command, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t OutBuffer[WORD_PADDED(2)] = {
        (uint8_t)((command & 0xFF00) >> 8),
        (uint8_t)(command & 0x00FF)};

    WaitOnBusy(radioNumber);
    SPIEx.read(radioNumber, OutBuffer, 2);
    return OutBuffer[0] << 8 | OutBuffer[1];
}

uint16_t ICACHE_RAM_ATTR LR2021Hal::ReadCommand(uint8_t *buffer, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t InBuffer[WORD_PADDED(size)] = {};

    memcpy(InBuffer, buffer, size);

    WaitOnBusy(radioNumber);
    SPIEx.read(radioNumber, InBuffer, size);

    memcpy(buffer, InBuffer, size);
    return InBuffer[0] << 8 | InBuffer[1];
}

bool ICACHE_RAM_ATTR LR2021Hal::WaitOnBusy(SX12XX_Radio_Number_t radioNumber)
{
    constexpr uint32_t wtimeoutUS = 2000U; // changed due to unknown issues in a small percentage of LR2021s that some
                                           // commands may have extremely long busy times during startup, exceeding 1ms
    uint32_t startTime = 0;

    while (true)
    {
        if (radioNumber == SX12XX_Radio_1)
        {
            if (digitalRead(GPIO_PIN_BUSY) == LOW)
            {
                return true;
            }
        }
        else if (radioNumber == SX12XX_Radio_2)
        {
            if (GPIO_PIN_BUSY_2 == UNDEF_PIN || digitalRead(GPIO_PIN_BUSY_2) == LOW)
            {
                return true;
            }
        }
        else if (radioNumber == SX12XX_Radio_All)
        {
            if (GPIO_PIN_BUSY_2 != UNDEF_PIN)
            {
                if (digitalRead(GPIO_PIN_BUSY) == LOW && digitalRead(GPIO_PIN_BUSY_2) == LOW)
                {
                    return true;
                }
            }
            else
            {
                if (digitalRead(GPIO_PIN_BUSY) == LOW)
                {
                    return true;
                }
            }
        }
        // Use this time to call micros().
        const uint32_t now = micros();
        if (startTime == 0)
        {
            startTime = now;
        }
        if ((now - startTime) > wtimeoutUS)
        {
            return false;
        }
    }
}

void ICACHE_RAM_ATTR LR2021Hal::dioISR_1()
{
    if (instance->IsrCallback_1)
    {
        instance->IsrCallback_1();
    }
}

void ICACHE_RAM_ATTR LR2021Hal::dioISR_2()
{
    if (instance->IsrCallback_2)
    {
        instance->IsrCallback_2();
    }
}

#endif // UNIT_TEST

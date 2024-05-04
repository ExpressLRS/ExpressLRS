#ifndef UNIT_TEST

#include "SX127xHal.h"
#include "SX127xRegs.h"
#include "logging.h"
#include <SPIEx.h>

SX127xHal *SX127xHal::instance = NULL;

SX127xHal::SX127xHal()
{
    instance = this;
}

void SX127xHal::end()
{
    detachInterrupt(GPIO_PIN_DIO0);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        detachInterrupt(GPIO_PIN_DIO0_2);
    }
    SPIEx.end();
    IsrCallback_1 = nullptr; // remove callbacks
    IsrCallback_2 = nullptr; // remove callbacks
}

void SX127xHal::init()
{
    DBGLN("Hal Init");

    pinMode(GPIO_PIN_DIO0, INPUT);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_DIO0_2, INPUT);
    }

    pinMode(GPIO_PIN_NSS, OUTPUT);
    digitalWrite(GPIO_PIN_NSS, HIGH);

#ifdef PLATFORM_ESP32
    SPIEx.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI, GPIO_PIN_NSS); // sck, miso, mosi, ss (ss can be any GPIO)
    gpio_pullup_en((gpio_num_t)GPIO_PIN_MISO);
    SPIEx.setFrequency(10000000);
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
    SPIEx.setFrequency(10000000);
#elif defined(PLATFORM_STM32)
    DBGLN("Config SPI");
    SPIEx.setBitOrder(MSBFIRST);
    SPIEx.setDataMode(SPI_MODE0);
    SPIEx.setMOSI(GPIO_PIN_MOSI);
    SPIEx.setMISO(GPIO_PIN_MISO);
    SPIEx.setSCLK(GPIO_PIN_SCK);
    SPIEx.begin();
    SPIEx.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
#endif

    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0), this->dioISR_1, RISING);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0_2), this->dioISR_2, RISING);
    }
}

void SX127xHal::reset(void)
{
    DBGLN("SX127x Reset");

    if (GPIO_PIN_RST != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_RST, OUTPUT);
        digitalWrite(GPIO_PIN_RST, LOW);
        if (GPIO_PIN_RST_2 != UNDEF_PIN)
        {
            pinMode(GPIO_PIN_RST_2, OUTPUT);
            digitalWrite(GPIO_PIN_RST_2, LOW);
        }
        delay(50); // Safety buffer. Busy takes longer to go low than the 1ms timeout in WaitOnBusy().
        pinMode(GPIO_PIN_RST, INPUT); // leave floating
        if (GPIO_PIN_RST_2 != UNDEF_PIN)
        {
            pinMode(GPIO_PIN_RST_2, INPUT);
        }
    }

    DBGLN("SX127x Ready!");
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegisterBits(uint8_t reg, uint8_t mask, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t rawValue = readRegister(reg, radioNumber);
    uint8_t maskedValue = rawValue & mask;
    return (maskedValue);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t data;
    readRegister(reg, &data, 1, radioNumber);
    return data;
}

void ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[WORD_PADDED(numBytes + 1)];
    buf[0] = reg | SPI_READ;

    SPIEx.read(radioNumber, buf, numBytes + 1);

    memcpy(data, buf + 1, numBytes);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegisterBits(uint8_t reg, uint8_t value, uint8_t mask, SX12XX_Radio_Number_t radioNumber)
{
    if (radioNumber & SX12XX_Radio_1)
    {
        uint8_t currentValue = readRegister(reg, SX12XX_Radio_1);
        uint8_t newValue = (currentValue & ~mask) | (value & mask);
        writeRegister(reg, newValue, SX12XX_Radio_1);
    }

    if (GPIO_PIN_NSS_2 != UNDEF_PIN && radioNumber & SX12XX_Radio_2)
    {
        uint8_t currentValue = readRegister(reg, SX12XX_Radio_2);
        uint8_t newValue = (currentValue & ~mask) | (value & mask);
        writeRegister(reg, newValue, SX12XX_Radio_2);
    }
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t data, SX12XX_Radio_Number_t radioNumber)
{
    writeRegister(reg, &data, 1, radioNumber);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[WORD_PADDED(numBytes + 1)];
    buf[0] = reg | SPI_WRITE;
    memcpy(buf + 1, data, numBytes);

    SPIEx.write(radioNumber, buf, numBytes + 1);
}

void ICACHE_RAM_ATTR SX127xHal::dioISR_1()
{
    if (instance->IsrCallback_1)
        instance->IsrCallback_1();
}

void ICACHE_RAM_ATTR SX127xHal::dioISR_2()
{
    if (instance->IsrCallback_2)
        instance->IsrCallback_2();
}

#endif // UNIT_TEST

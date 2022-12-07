#ifndef UNIT_TEST

#include "SX127xHal.h"
#include "SX127xRegs.h"
#include "logging.h"
#include <SPI.h>

SX127xHal *SX127xHal::instance = NULL;

SX127xHal::SX127xHal()
{
    instance = this;
}

void SX127xHal::end()
{
    TXRXdisable(); // make sure the RX/TX amp pins are disabled
    detachInterrupt(GPIO_PIN_DIO0);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        detachInterrupt(GPIO_PIN_DIO0_2);
    }
    SPI.end();
    IsrCallback_1 = nullptr; // remove callbacks
    IsrCallback_2 = nullptr; // remove callbacks
}

void SX127xHal::init()
{
    DBGLN("Hal Init");

#if defined(PLATFORM_ESP32)
#define SET_BIT(n) ((n != UNDEF_PIN) ? 1ULL << n : 0)

    txrx_disable_clr_bits = 0;
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_PA_ENABLE);
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE);
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE);
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE_2);
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE_2);

    tx1_enable_set_bits = 0;
    tx1_enable_clr_bits = 0;
    tx1_enable_set_bits |= SET_BIT(GPIO_PIN_PA_ENABLE);
    tx1_enable_set_bits |= SET_BIT(GPIO_PIN_TX_ENABLE);
    tx1_enable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE);
    tx1_enable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE_2);
    tx1_enable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE_2);

    tx2_enable_set_bits = 0;
    tx2_enable_clr_bits = 0;
    tx2_enable_set_bits |= SET_BIT(GPIO_PIN_PA_ENABLE);
    tx2_enable_set_bits |= SET_BIT(GPIO_PIN_TX_ENABLE_2);
    tx2_enable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE_2);
    tx2_enable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE);
    tx2_enable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE);

    rx_enable_set_bits = 0;
    rx_enable_clr_bits = 0;
    rx_enable_set_bits |= SET_BIT(GPIO_PIN_PA_ENABLE);
    rx_enable_set_bits |= SET_BIT(GPIO_PIN_RX_ENABLE);
    rx_enable_set_bits |= SET_BIT(GPIO_PIN_RX_ENABLE_2);
    rx_enable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE);
    rx_enable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE_2);
#else
    rx_enabled = false;
    tx1_enabled = false;
    tx2_enabled = false;
#endif

    pinMode(GPIO_PIN_DIO0, INPUT);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_DIO0_2, INPUT);
    }

    pinMode(GPIO_PIN_NSS, OUTPUT);
    digitalWrite(GPIO_PIN_NSS, HIGH);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_NSS_2, OUTPUT);
        digitalWrite(GPIO_PIN_NSS_2, HIGH);
    }

    if (GPIO_PIN_PA_ENABLE != UNDEF_PIN)
    {
        DBGLN("Use PA enable pin: %d", GPIO_PIN_PA_ENABLE);
        pinMode(GPIO_PIN_PA_ENABLE, OUTPUT);
        digitalWrite(GPIO_PIN_PA_ENABLE, LOW);
    }

    if (GPIO_PIN_TX_ENABLE != UNDEF_PIN)
    {
        DBGLN("Use TX pin: %d", GPIO_PIN_TX_ENABLE);
        pinMode(GPIO_PIN_TX_ENABLE, OUTPUT);
        digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
    }

    if (GPIO_PIN_RX_ENABLE != UNDEF_PIN)
    {
        DBGLN("Use RX pin: %d", GPIO_PIN_RX_ENABLE);
        pinMode(GPIO_PIN_RX_ENABLE, OUTPUT);
        digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
    }

    if (GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
    {
        DBGLN("Use TX_2 pin: %d", GPIO_PIN_TX_ENABLE_2);
        pinMode(GPIO_PIN_TX_ENABLE_2, OUTPUT);
        digitalWrite(GPIO_PIN_TX_ENABLE_2, LOW);
    }

    if (GPIO_PIN_RX_ENABLE_2 != UNDEF_PIN)
    {
        DBGLN("Use RX_2 pin: %d", GPIO_PIN_RX_ENABLE_2);
        pinMode(GPIO_PIN_RX_ENABLE_2, OUTPUT);
        digitalWrite(GPIO_PIN_RX_ENABLE_2, LOW);
    }

#ifdef PLATFORM_ESP32
    SPI.begin(GPIO_PIN_SCK, GPIO_PIN_MISO, GPIO_PIN_MOSI, GPIO_PIN_NSS); // sck, miso, mosi, ss (ss can be any GPIO)
    gpio_pullup_en((gpio_num_t)GPIO_PIN_MISO);
    SPI.setFrequency(10000000);
    SPI.setHwCs(true);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
        spiAttachSS(SPI.bus(), 1, GPIO_PIN_NSS_2);
    // spiEnableSSPins(SPI.bus(), SX12XX_Radio_All);
    spiEnableSSPins(SPI.bus(), 0xFF);
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

    attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0), this->dioISR_1, RISING);
    if (GPIO_PIN_DIO0_2 != UNDEF_PIN)
    {
        attachInterrupt(digitalPinToInterrupt(GPIO_PIN_DIO0_2), this->dioISR_2, RISING);
    }
}

void ICACHE_RAM_ATTR SX127xHal::setNss(uint8_t radioNumber, bool state)
{
#if defined(PLATFORM_ESP32)
    spiDisableSSPins(SPI.bus(), ~radioNumber);
    spiEnableSSPins(SPI.bus(), radioNumber);
#else
    if (radioNumber & SX12XX_Radio_1)
        digitalWrite(GPIO_PIN_NSS, state);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN && radioNumber & SX12XX_Radio_2)
        digitalWrite(GPIO_PIN_NSS_2, state);
#endif
}

void SX127xHal::reset(void)
{
    DBGLN("SX127x Reset");

    if (GPIO_PIN_RST != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_RST, OUTPUT);
        delay(100);
        digitalWrite(GPIO_PIN_RST, LOW);
        delay(100);
        pinMode(GPIO_PIN_RST, INPUT); // leave floating
    }

    DBGLN("SX127x Ready!");
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegisterValue(uint8_t reg, uint8_t msb, uint8_t lsb, SX12XX_Radio_Number_t radioNumber)
{
    if ((msb > 7) || (lsb > 7) || (lsb > msb))
    {
        DBGLN("ERROR INVALID BIT RANGE");
        return (ERR_INVALID_BIT_RANGE);
    }
    uint8_t rawValue = readRegister(reg);
    uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
    return (maskedValue);
}

uint8_t ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg, SX12XX_Radio_Number_t radioNumber)
{
    uint8_t data;
    readRegister(reg, &data, 1);
    return data;
}

void ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[numBytes + 1];
    buf[0] = reg | SPI_READ;

    setNss(radioNumber, LOW);
    SPI.transfer(buf, numBytes + 1);
    setNss(radioNumber, HIGH);

    memcpy(data, buf + 1, numBytes);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegisterValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb, SX12XX_Radio_Number_t radioNumber)
{
    if ((msb > 7) || (lsb > 7) || (lsb > msb))
    {
        DBGLN("ERROR INVALID BIT RANGE");
        return;
    }

    uint8_t currentValue = readRegister(reg);
    uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
    uint8_t newValue = (currentValue & ~mask) | (value & mask);
    writeRegister(reg, newValue);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t data, SX12XX_Radio_Number_t radioNumber)
{
    writeRegister(reg, &data, 1);
}

void ICACHE_RAM_ATTR SX127xHal::writeRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[numBytes + 1];
    buf[0] = reg | SPI_WRITE;
    memcpy(buf + 1, data, numBytes);

    setNss(radioNumber, LOW);
    SPI.transfer(buf, numBytes + 1);
    setNss(radioNumber, HIGH);
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

void ICACHE_RAM_ATTR SX127xHal::TXenable(SX12XX_Radio_Number_t radioNumber)
{
#if defined(PLATFORM_ESP32)
    if (radioNumber == SX12XX_Radio_2)
    {
        GPIO.out_w1ts = tx2_enable_set_bits;
        GPIO.out_w1tc = tx2_enable_clr_bits;

        GPIO.out1_w1ts.data = tx2_enable_set_bits >> 32;
        GPIO.out1_w1tc.data = tx2_enable_clr_bits >> 32;
    }
    else
    {
        GPIO.out_w1ts = tx1_enable_set_bits;
        GPIO.out_w1tc = tx1_enable_clr_bits;

        GPIO.out1_w1ts.data = tx1_enable_set_bits >> 32;
        GPIO.out1_w1tc.data = tx1_enable_clr_bits >> 32;
    }
#else
    if (!tx1_enabled && !tx2_enabled && !rx_enabled)
    {
        if (GPIO_PIN_PA_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_PA_ENABLE, HIGH);
    }
    if (rx_enabled)
    {
        if (GPIO_PIN_RX_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
        if (GPIO_PIN_RX_ENABLE_2 != UNDEF_PIN)
            digitalWrite(GPIO_PIN_RX_ENABLE_2, LOW);
        rx_enabled = false;
    }
    if (radioNumber == SX12XX_Radio_1 && !tx1_enabled)
    {
        if (GPIO_PIN_TX_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_TX_ENABLE, HIGH);
        if (GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
            digitalWrite(GPIO_PIN_TX_ENABLE_2, LOW);
        tx1_enabled = true;
        tx2_enabled = false;
    }
    if (radioNumber == SX12XX_Radio_2 && !tx2_enabled)
    {
        if (GPIO_PIN_TX_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
        if (GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
            digitalWrite(GPIO_PIN_TX_ENABLE_2, HIGH);
        tx1_enabled = false;
        tx2_enabled = true;
    }
#endif
}

void ICACHE_RAM_ATTR SX127xHal::RXenable()
{
#if defined(PLATFORM_ESP32)
    GPIO.out_w1ts = rx_enable_set_bits;
    GPIO.out_w1tc = rx_enable_clr_bits;

    GPIO.out1_w1ts.data = rx_enable_set_bits >> 32;
    GPIO.out1_w1tc.data = rx_enable_clr_bits >> 32;
#else
    if (!rx_enabled)
    {
        if (!tx1_enabled && !tx2_enabled && GPIO_PIN_PA_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_PA_ENABLE, HIGH);

        if (tx1_enabled && GPIO_PIN_TX_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
            tx1_enabled = false;
        }

        if (tx2_enabled && GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE_2, LOW);
            tx2_enabled = false;
        }

        if (GPIO_PIN_RX_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_RX_ENABLE, HIGH);
        if (GPIO_PIN_RX_ENABLE_2 != UNDEF_PIN)
            digitalWrite(GPIO_PIN_RX_ENABLE_2, HIGH);

        rx_enabled = true;
    }
#endif
}

void ICACHE_RAM_ATTR SX127xHal::TXRXdisable()
{
#if defined(PLATFORM_ESP32)
    GPIO.out_w1tc = txrx_disable_clr_bits;
    GPIO.out1_w1tc.data = txrx_disable_clr_bits >> 32;
#else
    if (rx_enabled)
    {
        if (GPIO_PIN_RX_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
        if (GPIO_PIN_RX_ENABLE_2 != UNDEF_PIN)
            digitalWrite(GPIO_PIN_RX_ENABLE_2, LOW);
        rx_enabled = false;
    }
    if (tx1_enabled)
    {
        if (GPIO_PIN_PA_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_PA_ENABLE, LOW);
        if (GPIO_PIN_TX_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
        tx1_enabled = false;
    }
    if (tx2_enabled)
    {
        if (GPIO_PIN_PA_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_PA_ENABLE, LOW);
        if (GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
            digitalWrite(GPIO_PIN_TX_ENABLE_2, LOW);
        tx2_enabled = false;
    }
#endif
}

#endif // UNIT_TEST

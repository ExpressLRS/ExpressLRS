#ifndef UNIT_TEST

#include "SX127xHal.h"
#include "SX127xRegs.h"
#include "logging.h"
#ifndef M0139
#include <SPIEx.h>
#else
#include <SPI.h>
#endif // M0139

SX127xHal *SX127xHal::instance = NULL;

#ifdef M0139
//SPIClass SPI_1(GPIO_PIN_MOSI, GPIO_PIN_MISO, GPIO_PIN_SCK, GPIO_PIN_NSS);
SPIClass SPI_1;
#ifdef DUAL_RADIO
//SPIClass SPI_2(GPIO_PIN_MOSI_2, GPIO_PIN_MISO_2, GPIO_PIN_SCK_2, GPIO_PIN_NSS_2);
SPIClass SPI_2;
#endif

void ICACHE_RAM_ATTR setNss(uint8_t radioNumber, bool state)
{
    if (radioNumber & SX12XX_Radio_1){
        // DBGLN("SETTING RADIO 1 NSS: %u", state);
        digitalWrite(GPIO_PIN_NSS, state);
    }
    if (GPIO_PIN_NSS_2 != UNDEF_PIN && radioNumber & SX12XX_Radio_2){
        // DBGLN("SETTING RADIO 2 NSS: %u", state);
        digitalWrite(GPIO_PIN_NSS_2, state);
    }
}
void ICACHE_RAM_ATTR resetNss(bool state)
{
    digitalWrite(GPIO_PIN_NSS, state);
    digitalWrite(GPIO_PIN_NSS_2, state);
}

#endif

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
    #if defined(M0139)
    SPI_1.end();
    #if defined(DUAL_RADIO)
    SPI_2.end();
    #endif
    #else
    SPIEx.end();
    #endif
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
    DBGLN("SPI_2 SSEL: %d", GPIO_PIN_NSS_2);
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        DBGLN("Initializing SPI_2");
        pinMode(GPIO_PIN_NSS_2, OUTPUT);
        digitalWrite(GPIO_PIN_NSS_2, HIGH);
    }
    
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
#elif defined(M0139)
    DBGLN("M0139");
    SPI_1.setMOSI(GPIO_PIN_MOSI);
    SPI_1.setMISO(GPIO_PIN_MISO);
    SPI_1.setSCLK(GPIO_PIN_SCK);
    SPI_1.setBitOrder(MSBFIRST);
    SPI_1.setDataMode(SPI_MODE0);
    //SPI_1.setSSEL(GPIO_PIN_NSS);
    SPI_1.begin();
    SPI_1.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
#if defined(DUAL_RADIO)
    DBGLN("DUAL M0139");
    SPI_2.setMOSI(GPIO_PIN_MOSI_2);
    SPI_2.setMISO(GPIO_PIN_MISO_2);
    SPI_2.setSCLK(GPIO_PIN_SCK_2);
    SPI_2.setBitOrder(MSBFIRST);
    SPI_2.setDataMode(SPI_MODE0);
    //SPI_2.setSSEL(GPIO_PIN_NSS_2);
    SPI_2.begin();
    SPI_2.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz
#endif
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
//        delay(50); // Safety buffer. Busy takes longer to go low than the 1ms timeout in WaitOnBusy().
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
    #ifdef NOT_DEF
    if(radioNumber == SX12XX_Radio_1)
    {
        return SPI_1.transfer(reg);
    } else if (radioNumber == SX12XX_Radio_2)
    {
        return SPI_2.transfer(reg);
    } else
    {
        SPI_1.transfer(reg);
        return SPI_2.transfer(reg);
    }
    #else
    uint8_t data;
    readRegister(reg, &data, 1, radioNumber);
    return data;
    #endif
}

void ICACHE_RAM_ATTR SX127xHal::readRegister(uint8_t reg, uint8_t *data, uint8_t numBytes, SX12XX_Radio_Number_t radioNumber)
{
    WORD_ALIGNED_ATTR uint8_t buf[WORD_PADDED(numBytes + 1)];
    buf[0] = reg | SPI_READ;

#ifdef M0139
    setNss(radioNumber, LOW);
    if (radioNumber == SX12XX_Radio_1){
        //digitalWrite(GPIO_PIN_NSS, LOW);
        //DBGLN("Reading Radio 1 SPI");
        SPI_1.transfer(buf, numBytes + 1);
        //digitalWrite(GPIO_PIN_NSS, HIGH);
        //DBGLN("SPI 1 OUT: 0x%x %x", buf[0], buf[1]);
    }
#ifdef DUAL_RADIO
    else if (radioNumber == SX12XX_Radio_2){
        //digitalWrite(GPIO_PIN_NSS_2, LOW);
        //DBGLN("Reading Radio 2 SPI");
        SPI_2.transfer(buf, numBytes + 1);
        //digitalWrite(GPIO_PIN_NSS_2, HIGH);
    } 
    else{
        //digitalWrite(GPIO_PIN_NSS, LOW);
        //DBGLN("Reading BOTH RADIOS");
        // DBGLN("Reading Radio 1 SPI");
        SPI_1.transfer(buf, numBytes + 1);
        //digitalWrite(GPIO_PIN_NSS, HIGH);

        //digitalWrite(GPIO_PIN_NSS_2, LOW);
        // DBGLN("Reading Radio 2 SPI");
        SPI_2.transfer(buf, numBytes + 1);
        //digitalWrite(GPIO_PIN_NSS_2, HIGH);
    }
#endif
    resetNss(HIGH);
#else
    SPIEx.read(radioNumber, buf, numBytes + 1);
#endif

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

#ifdef M0139
    setNss(radioNumber, LOW);
    if (radioNumber == SX12XX_Radio_1){
        //digitalWrite(GPIO_PIN_NSS, LOW);
        //DBGLN("Reading Radio 1 SPI");
        SPI_1.transfer(buf, numBytes + 1);
        //digitalWrite(GPIO_PIN_NSS, HIGH);
        //DBGLN("SPI 1 OUT: 0x%x %x", buf[0], buf[1]);
    }
#ifdef DUAL_RADIO
    else if (radioNumber == SX12XX_Radio_2){
        //digitalWrite(GPIO_PIN_NSS_2, LOW);
        //DBGLN("Reading Radio 2 SPI");
        SPI_2.transfer(buf, numBytes + 1);
        //digitalWrite(GPIO_PIN_NSS_2, HIGH);
    } 
    else{
        //digitalWrite(GPIO_PIN_NSS, LOW);
        //DBGLN("Reading BOTH RADIOS");
        // DBGLN("Reading Radio 1 SPI");
        SPI_1.transfer(buf, numBytes + 1);
        //digitalWrite(GPIO_PIN_NSS, HIGH);

        //digitalWrite(GPIO_PIN_NSS_2, LOW);
        // DBGLN("Reading Radio 2 SPI");
        SPI_2.transfer(buf, numBytes + 1);
        //digitalWrite(GPIO_PIN_NSS_2, HIGH);
    }
#endif
    resetNss(HIGH);
#else
    SPIEx.write(radioNumber, buf, numBytes + 1);
#endif
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

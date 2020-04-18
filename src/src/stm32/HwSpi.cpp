#include "HwSpi.h"
#include "targets.h"

HwSpi::HwSpi() : SPIClass()
{
    SS = GPIO_PIN_NSS;
    MOSI = GPIO_PIN_MOSI;
    MISO = GPIO_PIN_MISO;
    SCK = GPIO_PIN_SCK;
}

void HwSpi::platform_init(void)
{
    //SPI.setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz //not correct for SPI2
    setMOSI(MOSI);
    setMISO(MISO);
    setSCLK(SCK);
    setBitOrder(MSBFIRST);
    setDataMode(SPI_MODE0);
    SPIClass::begin();
    setClockDivider(SPI_CLOCK_DIV4); // 72 / 8 = 9 MHz //not correct for SPI2
}

void ICACHE_RAM_ATTR HwSpi::write(uint8_t data)
{
    SPIClass::transfer(data);
}

void ICACHE_RAM_ATTR HwSpi::write(uint8_t *data, uint8_t numBytes)
{
    SPIClass::transfer((uint8_t *)data, numBytes);
}

HwSpi RadioSpi;

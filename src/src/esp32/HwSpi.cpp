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
    // sck, miso, mosi, ss (ss can be any GPIO)
    SPIClass::begin(SCK, MISO, MOSI, -1);
    SPIClass::setDataMode(SPI_MODE0); // mode0 by default
    SPIClass::setFrequency(SX127X_SPI_SPEED);
}

void HwSpi::write(uint8_t data)
{
    taskDISABLE_INTERRUPTS();
    SPIClass::write(data);
    taskENABLE_INTERRUPTS();
}

void HwSpi::write(uint8_t *data, uint8_t numBytes)
{
    taskDISABLE_INTERRUPTS();
    SPIClass::writeBytes((uint8_t *)data, numBytes);
    taskENABLE_INTERRUPTS();
}

HwSpi RadioSpi;

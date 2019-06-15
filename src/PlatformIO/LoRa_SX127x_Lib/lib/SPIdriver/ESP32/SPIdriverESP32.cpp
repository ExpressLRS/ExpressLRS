#ifdef PLATFORM_ESP32_
#include <SPI.h>
#include <Arduino.h>
#include "SPIdriverESP32.h"

void IRAM_ATTR iSPI::init(int mosiPin, int misoPin, int clkPin, int csPin)
{
    //pinMode(CSPIN, OUTPUT);
    SPI.begin(clkPin, misoPin, mosiPin, csPin); // sck, miso, mosi, ss (ss can be any GPIO)
}

void IRAM_ATTR iSPI::setHost(spi_host_device_t host)
{
}

void IRAM_ATTR iSPI::transfer(uint8_t *data, size_t dataLen)
{
}

void IRAM_ATTR iSPI::transferNR(uint8_t *data, size_t dataLen)
{
}

void IRAM_ATTR iSPI::transferByteFIFO(const volatile uint8_t *data, uint8_t length)
{
}

uint8_t IRAM_ATTR iSPI::transferByte(uint8_t value)
{
    return 0;
}
#endif
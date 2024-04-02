#include "SPIEx.h"

#if defined(PLATFORM_ESP32)
#include <soc/spi_struct.h>
#endif

void ICACHE_RAM_ATTR SPIExClass::_transfer(uint8_t cs_mask, uint8_t *data, uint32_t size, bool reading)
{
#if defined(PLATFORM_ESP32)
    spi_dev_t *spi = *(reinterpret_cast<spi_dev_t**>(bus()));
    // wait for SPI to become non-busy
    while(spi->cmd.usr) {}

    // Set the CS pins which we want controlled by the SPI module for this operation
    spiDisableSSPins(bus(), ~cs_mask);
    spiEnableSSPins(bus(), cs_mask);

#if defined(PLATFORM_ESP32_S3)
    spi->ms_dlen.ms_data_bitlen = (size*8)-1;
#else
    spi->mosi_dlen.usr_mosi_dbitlen = ((size * 8) - 1);
    spi->miso_dlen.usr_miso_dbitlen = ((size * 8) - 1);
#endif

    // write the data to the SPI FIFO
    const uint32_t words = (size + 3) / 4;
    auto * const wordsBuf = reinterpret_cast<uint32_t *>(data);
    for(int i=0; i<words; i++)
    {
        spi->data_buf[i] = wordsBuf[i]; //copy buffer to spi fifo
    }

#if defined(PLATFORM_ESP32_S3)
    spi->cmd.update = 1;
    while (spi->cmd.update);
#endif
    // start the SPI module
    spi->cmd.usr = 1;

    if (reading)
    {
        // wait for SPI write to complete
        while(spi->cmd.usr) {}

        for(int i=0; i<words; i++)
        {
            wordsBuf[i] = spi->data_buf[i]; //copy spi fifo to buffer
        }
    }
#elif defined(PLATFORM_ESP8266)
    // we support only one hardware controlled CS pin, so theres nothing to do to configure it at this point

    // wait for SPI to become non-busy
    while(SPI1CMD & SPIBUSY) {}

    // Set in/out Bits to transfer
    const uint32_t mask = ~((SPIMMOSI << SPILMOSI) | (SPIMMISO << SPILMISO));
    const auto bits = (size * 8) - 1;
    SPI1U1 = ((SPI1U1 & mask) | ((bits << SPILMOSI) | (bits << SPILMISO)));

    // write the data to the SPI FIFO
    volatile uint32_t * const fifoPtr = &SPI1W0;
    const uint8_t outSize = ((size + 3) / 4);
    uint32_t * const dataPtr = (uint32_t*) data;
    for(int i=0; i<outSize; i++)
    {
        fifoPtr[i] = dataPtr[i];
    }

    // start the SPI module
    SPI1CMD |= SPIBUSY;

    if (reading)
    {
        // wait for SPI write to complete
        while(SPI1CMD & SPIBUSY) {}

        // read data from the FIFO back to the application buffer
        for(int i=0; i<outSize; i++)
        {
            dataPtr[i] = fifoPtr[i];
        }
    }
#else
    // only one (software-controlled) CS pin supported on STM32 devices, so set the state of the pin
    digitalWrite(GPIO_PIN_NSS, LOW);
    transfer(data, size);
    digitalWrite(GPIO_PIN_NSS, HIGH);
#endif
}

#if defined(PLATFORM_ESP32_S3)
SPIExClass SPIEx(FSPI);
#elif defined(PLATFORM_ESP32)
SPIExClass SPIEx(VSPI);
#else
SPIExClass SPIEx;
#endif

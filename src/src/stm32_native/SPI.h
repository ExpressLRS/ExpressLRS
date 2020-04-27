#pragma once

#include "gpio.h"

#include <stdint.h>
#include <stddef.h>

class SPIClass
{
public:
    SPIClass() {}

    void Begin(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t ssel = (uint8_t)-1);

    uint8_t transfer(uint32_t _buf);
    void transfer(void *_buf, size_t _count);

private:
    struct spi_config spi_cfg;
};

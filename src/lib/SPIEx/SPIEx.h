#include "targets.h"
#include <SPI.h>

class SPIExClass : public SPIClass
{
public:
#if defined(PLATFORM_ESP32)
    explicit SPIExClass(uint8_t spi_bus=HSPI) : SPIClass(spi_bus) {}
#else
    explicit SPIExClass() : SPIClass() {}
#endif

    void inline ICACHE_RAM_ATTR read(uint8_t cs_mask, uint8_t *data, uint32_t size) { _transfer(cs_mask, data, size, true); }
    void inline ICACHE_RAM_ATTR write(uint8_t cs_mask, uint8_t * data, uint32_t size) { _transfer(cs_mask, data, size, false); }

private:
    void _transfer(uint8_t cs_mask, uint8_t *data, uint32_t size, bool reading);
};

extern SPIExClass SPIEx;

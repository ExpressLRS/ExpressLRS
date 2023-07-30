#include <SPI.h>

class SPIExClass : public SPIClass
{
public:
    void inline ICACHE_RAM_ATTR read(uint8_t *data, uint32_t size) { transfer(data, size, true); }
    void inline ICACHE_RAM_ATTR write(uint8_t * data, uint32_t size) { transfer(data, size, false); }

private:
    void transfer(uint8_t *data, uint32_t size, bool reading);
};

extern SPIExClass SPIEx;

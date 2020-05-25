#ifndef RADIO_HAL_SPI_H_
#define RADIO_HAL_SPI_H_

#include "platform.h"
#include "HwSpi.h"
#include <stdint.h>

class RadioHalSpi {
public:
    RadioHalSpi(HwSpi &spi, uint8_t read, uint8_t write)
            : spi_bus(spi), p_write(write), p_read(read) {}

protected:
    void Begin(void);

    uint8_t ICACHE_RAM_ATTR getRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0) const;

    void ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes) const;

    uint8_t ICACHE_RAM_ATTR readRegister(uint8_t reg) const;
    void ICACHE_RAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0) const;

    void ICACHE_RAM_ATTR writeRegisterBurst(uint8_t reg, uint8_t *data, uint8_t numBytes) const;
    void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t data) const;

private:
    HwSpi &spi_bus;
    const uint8_t p_write = 0;
    const uint8_t p_read = 0;
};

#endif /* RADIO_HAL_SPI_H_ */

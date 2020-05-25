#include "RadioHalSpi.h"

void RadioHalSpi::Begin(void)
{
    spi_bus.prepare();
}

uint8_t ICACHE_RAM_ATTR RadioHalSpi::getRegValue(uint8_t reg, uint8_t msb, uint8_t lsb) const
{
    uint8_t value = readRegister(reg);
    if ((msb - lsb) < 7)
    {
        value = value & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
    }
    return (value);
}

void ICACHE_RAM_ATTR RadioHalSpi::readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes) const
{
    spi_bus.set_ss(LOW);
    spi_bus.write((reg | p_read));
    spi_bus.transfer(inBytes, numBytes);
    spi_bus.set_ss(HIGH);
}

uint8_t ICACHE_RAM_ATTR RadioHalSpi::readRegister(uint8_t reg) const
{
    uint8_t InByte;
    spi_bus.set_ss(LOW);
    spi_bus.write((reg | p_read));
    InByte = spi_bus.transfer(0x00);
    spi_bus.set_ss(HIGH);

    return (InByte);
}

void ICACHE_RAM_ATTR RadioHalSpi::setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb) const
{
    if ((msb - lsb) < 7)
    {
        uint8_t currentValue = readRegister(reg);
        uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
        value = (currentValue & ~mask) | (value & mask);
    }
    writeRegister(reg, value);
}

void ICACHE_RAM_ATTR RadioHalSpi::writeRegisterBurst(uint8_t reg, uint8_t *data, uint8_t numBytes) const
{
    spi_bus.set_ss(LOW);
    spi_bus.write(reg | p_write);
    spi_bus.write(data, numBytes);
    spi_bus.set_ss(HIGH);
}

void ICACHE_RAM_ATTR RadioHalSpi::writeRegister(uint8_t reg, uint8_t data) const
{
    spi_bus.set_ss(LOW);
    spi_bus.write(reg | p_write);
    spi_bus.write(data);
    spi_bus.set_ss(HIGH);
}

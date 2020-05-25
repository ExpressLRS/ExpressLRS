#include "LoRa_lowlevel.h"
#include "HwSpi.h"

void LoRaSpi::Begin(void)
{
    RadioSpi.prepare();
}

uint8_t ICACHE_RAM_ATTR LoRaSpi::getRegValue(uint8_t reg, uint8_t msb, uint8_t lsb) const
{
    uint8_t value = readRegister(reg);
    if ((msb - lsb) < 7)
    {
        value = value & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
    }
    return (value);
}

void ICACHE_RAM_ATTR LoRaSpi::readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes) const
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write((reg | p_read));
    RadioSpi.transfer(inBytes, numBytes);
    RadioSpi.set_ss(HIGH);
}

uint8_t ICACHE_RAM_ATTR LoRaSpi::readRegister(uint8_t reg) const
{
    uint8_t InByte;
    RadioSpi.set_ss(LOW);
    RadioSpi.write((reg | p_read));
    InByte = RadioSpi.transfer(0x00);
    RadioSpi.set_ss(HIGH);

    return (InByte);
}

void ICACHE_RAM_ATTR LoRaSpi::setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb) const
{
    if ((msb - lsb) < 7)
    {
        uint8_t currentValue = readRegister(reg);
        uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
        value = (currentValue & ~mask) | (value & mask);
    }
    writeRegister(reg, value);
}

void ICACHE_RAM_ATTR LoRaSpi::writeRegisterBurstStr(uint8_t reg, uint8_t *data, uint8_t numBytes) const
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write(reg | p_write);
    RadioSpi.write(data, numBytes);
    RadioSpi.set_ss(HIGH);
}

void ICACHE_RAM_ATTR LoRaSpi::writeRegister(uint8_t reg, uint8_t data) const
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write(reg | p_write);
    RadioSpi.write(data);
    RadioSpi.set_ss(HIGH);
}

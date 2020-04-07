#include "LoRa_lowlevel.h"
#include "../../src/debug.h"
#include "LoRa_SX127x.h"
#include "HwSpi.h"

void initModule()
{
    RadioSpi.prepare();
}

uint8_t ICACHE_RAM_ATTR getRegValue(uint8_t reg, uint8_t msb, uint8_t lsb)
{
    if ((msb > 7) || (lsb > 7) || (lsb > msb))
    {
        return (ERR_INVALID_BIT_RANGE);
    }
    uint8_t rawValue = readRegister(reg);
    uint8_t maskedValue = rawValue & ((0b11111111 << lsb) & (0b11111111 >> (7 - msb)));
    return (maskedValue);
}

uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes)
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write((reg | SPI_READ));
    RadioSpi.transfer(inBytes, numBytes);
    RadioSpi.set_ss(HIGH);

#if (DebugVerbosity >= 4)
    {
        DEBUG_PRINT("SPI: Read Burst ");
        DEBUG_PRINT("REG: ");
        DEBUG_PRINT(reg);
        DEBUG_PRINT(" LEN: ");
        DEBUG_PRINT(numBytes);
        DEBUG_PRINT(" DATA: ");

        for (int i = 0; i < numBytes; i++)
        {
            DEBUG_PRINT(inBytes[i]);
        }

        DEBUG_PRINTLN();
    }
#endif

    return (ERR_NONE);
}

/*
uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, volatile uint8_t *inBytes)
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write((reg | SPI_READ));
    RadioSpi.transfer((uint8_t *)inBytes, numBytes);
    RadioSpi.set_ss(HIGH);

#if (DebugVerbosity >= 4)
    {
        DEBUG_PRINT("SPI: Read Burst ");
        DEBUG_PRINT("REG: ");
        DEBUG_PRINT(reg);
        DEBUG_PRINT(" LEN: ");
        DEBUG_PRINT(numBytes);
        DEBUG_PRINT(" DATA: ");

        for (int i = 0; i < numBytes; i++)
        {
            DEBUG_PRINT(inBytes[i]);
        }

        DEBUG_PRINTLN();
    }
#endif
    return (ERR_NONE);
}
*/

/*
uint8_t ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, char *inBytes)
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write(reg | SPI_READ);
    RadioSpi.transfer((uint8_t *)inBytes, numBytes);
    RadioSpi.set_ss(HIGH);

#if (DebugVerbosity >= 4)
    {
        DEBUG_PRINT("SPI: Read BurstStr ");
        DEBUG_PRINT("REG: ");
        DEBUG_PRINT(reg);
        DEBUG_PRINT(" LEN: ");
        DEBUG_PRINT(numBytes);
        DEBUG_PRINT(" DATA: ");

        for (int i = 0; i < numBytes; i++)
        {
            DEBUG_PRINT(inBytes[i]);
        }

        DEBUG_PRINTLN();
    }
#endif
    return (ERR_NONE);
}
*/

uint8_t ICACHE_RAM_ATTR readRegister(uint8_t reg)
{
    uint8_t InByte;
    RadioSpi.set_ss(LOW);
    RadioSpi.write((reg | SPI_READ));
    InByte = RadioSpi.transfer(0x00);
    RadioSpi.set_ss(HIGH);

    return (InByte);
}

uint8_t ICACHE_RAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb, uint8_t lsb)
{
    if ((msb > 7) || (lsb > 7) || (lsb > msb))
    {
        return (ERR_INVALID_BIT_RANGE);
    }

    uint8_t currentValue = readRegister(reg);
    uint8_t mask = ~((0b11111111 << (msb + 1)) | (0b11111111 >> (8 - lsb)));
    uint8_t newValue = (currentValue & ~mask) | (value & mask);
    writeRegister(reg, newValue);
    return (ERR_NONE);
}

void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, uint8_t *data, uint8_t numBytes)
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write(reg | SPI_WRITE);
    RadioSpi.write(data, numBytes);
    RadioSpi.set_ss(HIGH);
}

/*
void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, const volatile uint8_t *data, uint8_t numBytes)
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write(reg | SPI_WRITE);
    RadioSpi.write((uint8_t *)data, numBytes);
    RadioSpi.set_ss(HIGH);
}
*/

void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t data)
{
    RadioSpi.set_ss(LOW);
    RadioSpi.write(reg | SPI_WRITE);
    RadioSpi.write(data);
    RadioSpi.set_ss(HIGH);

#if (0 && DebugVerbosity >= 4)
    {
        DEBUG_PRINT("SPI: Write ");
        DEBUG_PRINT("REG: ");
        DEBUG_PRINT(reg, HEX);
        DEBUG_PRINT(" VAL: ");
        DEBUG_PRINTLN(data, HEX);
    }
#endif
}

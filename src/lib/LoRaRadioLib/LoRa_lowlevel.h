#ifndef LoRa_lowlevel
#define LoRa_lowlevel

#include "platform.h"
#include <stdint.h>

class LoRaSpi {
public:
    LoRaSpi(uint8_t read, uint8_t write) {
        p_read = read;
        p_write = write;
    }
protected:
    void Begin(void);

    uint8_t ICACHE_RAM_ATTR getRegValue(uint8_t reg, uint8_t msb = 7, uint8_t lsb = 0) const;

    void ICACHE_RAM_ATTR readRegisterBurst(uint8_t reg, uint8_t numBytes, uint8_t *inBytes) const;

    uint8_t ICACHE_RAM_ATTR readRegister(uint8_t reg) const;
    void ICACHE_RAM_ATTR setRegValue(uint8_t reg, uint8_t value, uint8_t msb = 7, uint8_t lsb = 0) const;

    void ICACHE_RAM_ATTR writeRegisterBurstStr(uint8_t reg, uint8_t *data, uint8_t numBytes) const;
    void ICACHE_RAM_ATTR writeRegister(uint8_t reg, uint8_t data) const;

private:
    uint8_t p_write = 0;
    uint8_t p_read = 0;
};

#endif

#ifndef WIRE_H_
#define WIRE_H_

#include "Arduino.h"

class I2C
{
public:
    I2C();
    void setSDA(uint32_t sda);
    void setSCL(uint32_t scl);
    void begin();
    void beginTransmission(uint8_t addr);
    void write(uint8_t data);
    void endTransmission();

private:
    uint32_t sda_pin = 0;
    uint32_t scl_pin = 0;
    void *i2c = NULL;
    uint8_t address = 0;
};

extern I2C Wire;

#endif /* WIRE_H_ */

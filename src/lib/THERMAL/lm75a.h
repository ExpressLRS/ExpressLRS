#pragma once

#include "targets.h"
#include "Wire.h"

class LM75A
{
private:
    void ReadAccRegister(uint8_t reg, uint8_t *data, int size);
    void WriteAccRegister(uint8_t reg, uint8_t *data, int size);
public:
    int init();
    uint8_t read_lm75a();
    void update_lm75a_threshold(uint8_t tos, uint8_t thyst);
};

#define LM75A_I2C_ADDRESS 0x48

#define LM75A_REG_TEMP 0x00
#define LM75A_REG_CONF 0x01

#define LM75A_REG_THYST 0x02
#define LM75A_REG_TOS   0x03
#define LM75A_TOS_DEFAULT_VALUE 0x50

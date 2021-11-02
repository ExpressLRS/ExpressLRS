#pragma once

#include "targets.h"
#include "Wire.h"

class STK8xxx
{
private:
    void ReadAccRegister(uint8_t reg, uint8_t *data);
    void WriteAccRegister(uint8_t reg, uint8_t data);
    void STK8xxx_Suspend_mode();
    bool STK8xxx_Check_chipid();              
public:
    void STK8xxx_Anymotion_init();
    void STK8xxx_Sigmotion_init();
    void STK8xxx_Disable_Motion();
    int STK8xxx_Initialization();
    int STK8xxx_Get_Sensitivity();
    void STK8xxx_Getregister_data(float *X_DataOut, float *Y_DataOut, float *Z_DataOut);
};

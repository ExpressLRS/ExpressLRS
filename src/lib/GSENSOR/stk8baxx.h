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

#define STK8xxx_SLAVE_ADDRESS	0x18

#define STK_REG_CHIPID		0x00
/* STK8321 or STK8323 CHIP ID = 0x23 */
#define STK8xxx_CHIPID_VAL  0x23
#define STK8327_CHIPID_VAL  0x26
/* STK8BAxx */
 /* S or R resolution = 10 bit */
#define STK8BA50_X_CHIPID_VAL   0x86
#define STK8BA5X_CHIPID_VAL     0x87

#define	STK8xxx_REG_POWMODE 0x11
#define	STK_SUSPEND_MODE    0x80

#define STK8xxx_REG_RANGESEL	0x0F
#define STK8xxx_RANGE_2G		0x03
#define STK8xxx_RANGE_4G		0x05
#define STK8xxx_RANGE_8G		0x08

#define STK8xxx_REG_XOUT1    0x02
#define STK8xxx_REG_XOUT2    0x03
#define STK8xxx_REG_YOUT1    0x04
#define STK8xxx_REG_YOUT2    0x05
#define STK8xxx_REG_ZOUT1    0x06
#define STK8xxx_REG_ZOUT2    0x07

#define STK8xxx_REG_BWSEL    0x10
#define STK8xxx_REG_SWRST    0x14
#define STK8xxx_REG_INTEN1   0x16
#define STK8xxx_REG_INTMAP1  0x19
#define STK8xxx_REG_INTCFG1  0x20
#define STK8xxx_REG_SLOPETHD 0x28
#define STK8xxx_REG_SIGMOT2  0x2A
#define STK8xxx_REG_INTFCFG  0x34


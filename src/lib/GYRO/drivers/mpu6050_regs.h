#pragma once

#define MPU6050_ADDRESS_AD0_LOW     0x68 // address pin low (GND), default for InvenSense evaluation board
#define MPU6050_ADDRESS_AD0_HIGH    0x69 // address pin high (VCC)
#define MPU6050_DEFAULT_ADDRESS     MPU6050_ADDRESS_AD0_LOW


// Registers
#define MPU6050_RA_CONFIG           0x1A
#define MPU6050_RA_GYRO_CONFIG      0x1B
#define MPU6050_RA_ACCEL_CONFIG     0x1C
#define MPU6050_RA_I2C_MST_CTRL     0x24
#define MPU6050_RA_INT_PIN_CFG      0x37
#define MPU6050_RA_INT_ENABLE       0x38
#define MPU6050_RA_INT_STATUS       0x3A
#define MPU6050_RA_ACCEL_XOUT_H     0x3B
#define MPU6050_RA_PWR_MGMT_1       0x6B
#define MPU6050_RA_WHO_AM_I         0x75


// Status Register
#define MPU6050_INTERRUPT_DATA_RDY_BIT      0           // Bit 0: Data Ready

// PWR_MGMT_1
#define MPU6050_PWR1_DEVICE_RESET_BIT   7             // Bit 7
#define MPU6050_PWR1_SLEEP_BIT          6             // bit 6
#define MPU6050_PWR1_CLKSEL_BIT         0             // Bit 0, len=3

// Power MAnagement value (3 bits)
#define MPU6050_CLOCK_PLL_XGYRO         0x01

// Accel Config Bits
#define MPU6050_ACONFIG_XA_ST_BIT           7
#define MPU6050_ACONFIG_YA_ST_BIT           6
#define MPU6050_ACONFIG_ZA_ST_BIT           5
#define MPU6050_ACONFIG_AFS_SEL_BIT         3         //Bit 3, Len=2
#define MPU6050_ACONFIG_ACCEL_HPF_BIT       0         //Bit 0, Len=3


// Accel Config Values
#define MPU6050_ACCEL_FS_2          0x00
#define MPU6050_ACCEL_FS_4          0x01
#define MPU6050_ACCEL_FS_8          0x02
#define MPU6050_ACCEL_FS_16         0x03

// Gyro Config Bits
#define MPU6050_GCONFIG_FS_SEL_BIT      3          //Bit 3, Len=2

// Gyro Config Values
#define MPU6050_GYRO_FS_250         0x00
#define MPU6050_GYRO_FS_500         0x01
#define MPU6050_GYRO_FS_1000        0x02
#define MPU6050_GYRO_FS_2000        0x03


// I2C_Config 
#define MPU6050_I2C_MST_CLK_BIT     3
#define MPU6050_I2C_MST_CLK_LENGTH  4

// Values
#define MPU6050_CLOCK_DIV_400       0xD

//Int Enable Reg
#define MPU6050_INTERRUPT_DATA_RDY_BIT      0

// RA Config Register
#define MPU6050_CFG_DLPF_CFG_BIT    0           // Bit 0, Len=3

// Values
#define MPU6050_DLPF_BW_256         0x00
#define MPU6050_DLPF_BW_188         0x01
#define MPU6050_DLPF_BW_98          0x02
#define MPU6050_DLPF_BW_42          0x03
#define MPU6050_DLPF_BW_20          0x04
#define MPU6050_DLPF_BW_10          0x05
#define MPU6050_DLPF_BW_5           0x06
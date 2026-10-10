#pragma once
#include "targets.h"

static const uint8_t I2C_ADDRESS_SDO_LOW = 0x18;
static const uint8_t I2C_ADDRESS_SDO_HIGH = 0x19;

// Registers
static const uint8_t REG_WHO_AM_I = 0x01;
static const uint8_t REG_COM_CONF = 0x04;
static const uint8_t REG_INT1_OUT_SEL1 = 0x05;
static const uint8_t REG_DATA_STAT = 0x0B;
static const uint8_t REG_ACC_XH = 0x0C;
static const uint8_t REG_ACC_CONF = 0x40;
static const uint8_t REG_ACC_RANGE = 0x41;
static const uint8_t REG_GYR_CONF = 0x42;
static const uint8_t REG_GYR_RANGE = 0x43;
static const uint8_t REG_SOFT_RST = 0x4A;
static const uint8_t REG_PWR_CTRL = 0x7D;
static const uint8_t REG_SEG_SEL = 0x7F;


static const uint8_t COM_CONF_BOOT = 0x80;      // Force memory initialization
static const uint8_t COM_CONF_BDU = 0x40;       // Block-Data-Update
static const uint8_t COM_CONF_ADDR_AUTO = 0x10; // Address auto increment when reading

static const uint8_t INT1_OUT_SEL1_ACC = 0x01; // Int1 on Gyro data ready
static const uint8_t INT1_OUT_SEL1_GYR = 0x04; // Int1 on Gyro data ready

static const uint8_t DATA_STAT_DRDY_ACC = 0x01; // Acceleromenter Data ready
static const uint8_t DATA_STAT_DRDY_GYR = 0x02; // Gyro Data ready

static const uint8_t PWR_CTRL_TEMP_EN = 0x08;
static const uint8_t PWR_CTRL_ACC_EN = 0x04;
static const uint8_t PWR_CTRL_GYR_EN = 0x02;

static const uint8_t ACC_CONF_FILTER_PERF_HIGH = 0x80;   // High performance mode ON
static const uint8_t ACC_CONF_BWP_OSR4_AVG1 = 0x00;      // Filter Bandwidth: average times=1.6kHz/ODR   
static const uint8_t ACC_CONF_BWP_OSR2_AVG2 = 0x10;      // Filter Bandwidth: average times=1.6kHz/(ODR*2)
static const uint8_t ACC_CONF_BWP_NORM_AVG4 = 0x20;      // Filter Bandwidth: average times=1.6kHz/(ODR*4) 
static const uint8_t ACC_CONF_ODR_800  = 0x0B;           // ODR 800hz
static const uint8_t ACC_CONF_ODR_1600 = 0x0C;           // ODR 1600hz

static const uint8_t ACC_RANGE_16G = 0x03;     // Range 16g
static const uint8_t ACC_RANGE_8G  = 0x02;
static const uint8_t ACC_RANGE_4G  = 0x01;
static const uint8_t ACC_RANGE_2G  = 0x00;

static const uint8_t GYR_CONF_FILTER_PERF_HIGH = 0x80;
static const uint8_t GYR_CONF_NOISE_PERF_HIGH  = 0x40;   // Noise Optimization ON
static const uint8_t GYR_CONF_BWP_OSR4_AVG1 = 0x00;      // Filter Bandwidth: average times=3.2kHz/ODR
static const uint8_t GYR_CONF_BWP_OSR2_AVG2 = 0x10;      // Filter Bandwidth: average times=3.2kHz/(ODR*2) 
static const uint8_t GYR_CONF_BWP_NORM_AVG4 = 0x20;      // Filter Bandwidth: average times=3.2kHz/(ODR*4)
static const uint8_t GYR_CONF_ODR_800  = 0x0B;           // ODR 800hz
static const uint8_t GYR_CONF_ODR_1600 = 0x0C;           // ODR 1600hz

static const uint8_t GYR_RANGE_2000DPS = 0x00;
static const uint8_t GYR_RANGE_1000DPS = 0x01;
static const uint8_t GYR_RANGE_500DPS  = 0x02;

static const uint8_t CHIP_ID = 0x6A;
static const uint8_t SOFT_RESET_VALUE = 0xA5;

static const uint16_t RESET_DELAY_MS = 200;
static const uint8_t SENSOR_START_DELAY_MS = 60;
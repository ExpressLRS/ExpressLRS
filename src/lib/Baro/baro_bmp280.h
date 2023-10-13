#pragma once

#include "baro_base.h"
#include "baro_bmp280_regs.h"

class BMP280 : public BaroI2CBase
{
public:
    // Detect if chip is present
    static bool detect();

    // BaroBase methods
    void initialize();
    uint8_t getPressureDuration();
    void startPressure();
    uint32_t getPressure();
    // Temperature and Pressure are measured at the same time
    uint8_t getTemperatureDuration() { return 0; };
    void startTemperature() {};
    int32_t getTemperature();

protected:
    // Filter 4: 1x=1.2PaRMS 2x=1.0PaRMS 4x=0.8PaRMS 8x=0.6PaRMS 16x=0.5PaRMS
    const uint8_t FILTER_COEFF = BMP280_FILTER_COEFF_4; // 8 in iNav
    const uint8_t OVERSAMPLING_PRESSURE = BMP280_OVERSAMP_16X; // 8x in iNav
    const uint8_t OVERSAMPLING_TEMPERATURE = BMP280_OVERSAMP_1X;

    // Override from i2cbase
    static const uint8_t getI2CAddress() { return BMP280_I2C_ADDR; }

    // calibration data, if initialized
    // Packed to be able to read all at once
    struct tagCalibrationData
    {
        uint16_t dig_T1;
        int16_t dig_T2;
        int16_t dig_T3;
        uint16_t dig_P1;
        int16_t dig_P2;
        int16_t dig_P3;
        int16_t dig_P4;
        int16_t dig_P5;
        int16_t dig_P6;
        int16_t dig_P7;
        int16_t dig_P8;
        int16_t dig_P9;
    } __attribute__((packed)) m_calib;

    int32_t m_temperatureLast; // t_fine
};

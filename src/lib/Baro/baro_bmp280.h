#pragma once

#include "baro_base.h"
#include "baro_bmp280_regs.h"

class BMP280 : public BaroI2CBase
{
public:
    BMP280() : BaroI2CBase(), m_calib{0} {}

    // Detect if chip is present
    static bool detect();

    // BaroBase methods
    void initialize();
    uint8_t getTemperatureDuration();
    void startTemperature();
    int32_t getTemperature();
    // Temperature and Pressure are measured at the same time
    uint8_t getPressureDuration() { return 0; }
    void startPressure() {}
    uint32_t getPressure();

protected:
    // Filter 4: 1x=1.2PaRMS 2x=1.0PaRMS 4x=0.8PaRMS 8x=0.6PaRMS 16x=0.5PaRMS
    // Filter 8: 1x=0.9PaRMS 2x=0.6PaRMS 4x=0.5PaRMS 8x=0.4PaRMS 16x=0.4PaRMS
    // 16x pressure 1x Temp = 40.925ms
    // 8x pressure 1x Temp = 22.525ms
    static const uint8_t FILTER_COEFF = BMP280_FILTER_COEFF_8; // 8 in iNav
    static const uint8_t OVERSAMPLING_PRESSURE = BMP280_OVERSAMP_8X; // 8x in iNav
    static const uint8_t OVERSAMPLING_TEMPERATURE = BMP280_OVERSAMP_1X; // 1x in iNav
    static const uint8_t FREERUNNING_NUM_SAMPLES = 4; // Report after this number of samples complete

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

    uint32_t m_pressureLast;
};

#pragma once

#include "baro_base.h"

class SPL06 : public BaroBase
{
public:
    SPL06() : BaroBase(), m_calib{0} {}

    // Detect if chip is present
    static bool detect();

    void initialize();
    uint8_t getPressureDuration();
    void startPressure();
    uint32_t getPressure();
    uint8_t getTemperatureDuration();
    void startTemperature();
    int32_t getTemperature();
protected:
    // 32x Pressure + 8x Temperature = 70ms per update1
    // 4x=8.4ms/2.5PaRMS, 8x=14.8ms, 16x=27.6ms/1.2Pa, 32x=53.2ms/0.9Pa, 64x=104.4ms/0.5Pa
    const uint8_t OVERSAMPLING_PRESSURE = 32;
    const uint8_t OVERSAMPLING_TEMPERATURE = 8;

    static void readRegister(uint8_t reg, uint8_t *data, size_t size);
    static void writeRegister(uint8_t reg, uint8_t *data, size_t size);

    uint8_t oversampleToRegVal(const uint8_t oversamples) const;
    int32_t oversampleToScaleFactor(const uint8_t oversamples) const;
    float m_temperatureLast; // last uncompensated temperature value

    struct tagCalibrationData
    {
        int16_t c0;
        int16_t c1;
        int32_t c00;
        int32_t c10;
        int16_t c01;
        int16_t c11;
        int16_t c20;
        int16_t c21;
        int16_t c30;
    } m_calib; // calibration data, if initialized

};
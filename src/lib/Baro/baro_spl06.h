#pragma once

#include "baro_base.h"
#include "baro_spl06_regs.h"

class SPL06 : public BaroI2CBase<SPL06_I2C_ADDR>
{
public:
    SPL06() : BaroI2CBase(), m_calib{0} {}

    // Detect if chip is present
    static bool detect();

    // BaroBase methods
    void initialize();
    uint8_t getPressureDuration();
    void startPressure();
    uint32_t getPressure();
    uint8_t getTemperatureDuration();
    void startTemperature();
    int32_t getTemperature();
protected:
    // 32x Pressure + 8x Temperature = 70ms per update
    // 4x=8.4ms/2.5PaRMS, 8x=14.8ms, 16x=27.6ms/1.2Pa, 32x=53.2ms/0.9Pa, 64x=104.4ms/0.5Pa
    const uint8_t OVERSAMPLING_PRESSURE = 32;
    const uint8_t OVERSAMPLING_TEMPERATURE = 8;

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
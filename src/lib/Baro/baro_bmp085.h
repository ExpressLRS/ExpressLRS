#if 0
// BRY: I implemented this entire unit thinking I had a BMP085 on hand
// but I do not so this code is completely untested and never has run
// but it compiles, so ship it!
#pragma once

#include "baro_base.h"
#include "baro_bmp085_regs.h"

class BMP085 : public BaroI2CBase<BMP085_I2C_ADDR>
{
public:
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
    // 3x pressure + temperature = 30.5ms per update
    // 0x=4.5ms, 1x=7.5ms, 2x=13.5ms, 3x=25.5ms
    //             +3        +9         +21
    const uint8_t OVERSAMPLING_PRESSURE = 3;

    // calibration data, if initialized
    // Packed to be able to read all at once
    struct tagCalibrationData
    {
        int16_t ac1;
        int16_t ac2;
        int16_t ac3;
        uint16_t ac4;
        uint16_t ac5;
        uint16_t ac6;
        int16_t b1;
        int16_t b2;
        int16_t mb;
        int16_t mc;
        int16_t md;
    } __attribute__((packed)) m_calib;

    uint16_t m_temperatureLast;

private:
    int32_t computeB5(int32_t UT) const;
};
#endif
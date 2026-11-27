#pragma once

#include "baro_base.h"

class SPA06 : public BaroI2CBase
{
public:
    static bool detect();

    // BaroBase methods
    void initialize() override;
    uint8_t getPressureDuration() override;
    void startPressure() override;
    uint32_t getPressure() override;
    uint8_t getTemperatureDuration() override;
    void startTemperature() override;
    int32_t getTemperature() override;

private:
    bool m_resetIssued = false;
    float m_temperatureLast = 0; // last scaled, uncompensated temperature

    struct
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
        int16_t c31;
        int16_t c40;
    } m_calib{};
};

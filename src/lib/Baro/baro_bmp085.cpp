#if 0 // UNTESTED
#include "baro_bmp085.h"

/****
 * Calculations used in this code taken from Adafruit BMP085 calculation
 * https://github.com/adafruit/Adafruit-BMP085-Library/blob/master/Adafruit_BMP085.cpp
 *
 * Code reused under GPL License, see the Adafruit project for full license information
 * https://github.com/adafruit/Adafruit-BMP085-Library/
 ****/

void BMP085::initialize()
{
    if (m_initialized)
        return;

    // All the calibration registers are in order of our struct and are 2 bytes long too,
    // so just read them directly into our calib data
    readRegister(BMP085_CALIB_COEFFS_START, (uint8_t *)&m_calib, sizeof(m_calib));
}

uint8_t BMP085::getPressureDuration()
{
    switch (OVERSAMPLING_PRESSURE)
    {
        case 0: return 5; // 4.5ms
        case 1: return 8; // 7.5ms
        case 2: return 14; // 13.5ms
        default: // fallthrough
        case 3: return 26; // 25.5ms
    }
}

void BMP085::startPressure()
{
    uint8_t reg_value = BMP085_CMD_MEAS_PRESSURE | (OVERSAMPLING_PRESSURE << 6);
    writeRegister(BMP085_REG_CONTROL, &reg_value, sizeof(reg_value));
}

int32_t BMP085::computeB5(int32_t UT) const
{
    int32_t X1 = (UT - (int32_t)m_calib.ac6) * ((int32_t)m_calib.ac5) >> 15;
    int32_t X2 = ((int32_t)m_calib.mc << 11) / (X1 + (int32_t)m_calib.md);
    return X1 + X2;
}

uint32_t BMP085::getPressure()
{
    uint8_t buf[3];
    readRegister(BMP085_REG_PRESSURE_DATA, buf, sizeof(buf));
    uint32_t raw = ((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> OVERSAMPLING_PRESSURE;

    int32_t B3, B5, B6, X1, X2, X3, p;
    uint32_t B4, B7;

    B5 = computeB5(m_temperatureLast);

    B6 = B5 - 4000;
    X1 = ((int32_t)m_calib.b2 * ((B6 * B6) >> 12)) >> 11;
    X2 = ((int32_t)m_calib.ac2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = ((((int32_t)m_calib.ac1 * 4 + X3) << OVERSAMPLING_PRESSURE) + 2) / 4;

    X1 = ((int32_t)m_calib.ac3 * B6) >> 13;
    X2 = ((int32_t)m_calib.b1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = ((uint32_t)m_calib.ac4 * (uint32_t)(X3 + 32768)) >> 15;
    B7 = ((uint32_t)raw - B3) * (uint32_t)(50000UL >> OVERSAMPLING_PRESSURE);

    if (B7 < 0x80000000) {
        p = (B7 * 2) / B4;
    } else {
        p = (B7 / B4) * 2;
    }
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;

    p = p + ((X1 + X2 + (int32_t)3791) >> 4);
    return p;
}

uint8_t BMP085::getTemperatureDuration()
{
    // Max. conversion time 4.5ms
    return 5;
}

void BMP085::startTemperature()
{
    uint8_t reg_value = BMP085_CMD_MEAS_TEMPERATURE;
    writeRegister(BMP085_REG_CONTROL, &reg_value, sizeof(reg_value));
}

int32_t BMP085::getTemperature()
{
    m_temperatureLast = 0;
    readRegister(BMP085_REG_TEMPERATURE_DATA, (uint8_t *)&m_temperatureLast, sizeof(m_temperatureLast));
    return m_temperatureLast;
}

bool BMP085::detect()
{
    // Assumes Wire.begin() has already been called
    uint8_t chipid = 0;
    readRegister(BMP085_REG_CHIPID, &chipid, sizeof(chipid));
    return chipid == BMP085_CHIPID;
}
#endif
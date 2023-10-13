#include "baro_bmp280.h"

/****
 * Calculations used in this code taken from iNav
 * https://github.com/iNavFlight/inav/blob/master/src/main/drivers/barometer/barometer_bmp280.cpp
 *
 * Code reused under GPL License, see the iNav project for full license information
 * https://github.com/iNavFlight/inav
 ****/

void BMP280::initialize()
{
    if (m_initialized)
        return;

    // All the calibration registers are in order of our struct and are 2 bytes long too,
    // so just read them directly into our calib data
    readRegister(BMP280_REG_CALIB_COEFFS_START, (uint8_t *)&m_calib, sizeof(m_calib));

    // configure IIR filter
    const uint8_t BMP280_FILTER = FILTER_COEFF << 2;
    writeRegister(BMP280_REG_CONFIG, (uint8_t *)&BMP280_FILTER, sizeof(BMP280_FILTER));
}

uint8_t BMP280::getPressureDuration()
{
    // Calculate in microseconds, then divide by 1000 for ms, rounding up
    // BRY: Test that this is accurate?
    return (1250U +
        (2300U * ((1U << OVERSAMPLING_TEMPERATURE) >> 1)) +
        (2300U * ((1U << OVERSAMPLING_PRESSURE) >> 1) + 575U) +
        999U) / 1000U;
}

void BMP280::startPressure()
{
    // Measure both Pressure and Temperature, forced mode (not freerunning)
    const uint8_t BMP280_MODE = (OVERSAMPLING_PRESSURE << 2 | OVERSAMPLING_TEMPERATURE << 5 | BMP280_FORCED_MODE);
    writeRegister(BMP280_REG_CTRL_MEAS, (uint8_t *)&BMP280_MODE, sizeof(BMP280_MODE));
}

uint32_t BMP280::getPressure()
{
    uint8_t buf[BMP280_LEN_TEMP_PRESS_DATA] = {0};
    readRegister(BMP280_REG_PRESSURE_MSB, buf, sizeof(buf));

    uint32_t pressure = ((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4);
    int32_t temperature = ((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4);

    return BaroBase::PRESSURE_INVALID;
}

int32_t BMP280::getTemperature()
{
    return m_temperatureLast;
}

bool BMP280::detect()
{
    // Assumes Wire.begin() has already been called
    uint8_t chipid = 0;
    readRegister(BMP280_REG_CHIPID, &chipid, sizeof(chipid));
    return (chipid == BMP280_CHIPID || chipid == BME280_CHIPID);
}

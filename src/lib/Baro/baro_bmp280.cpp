#include "baro_bmp280.h"
#include <Arduino.h>
#include "logging.h"

void BMP280::initialize()
{
    if (m_initialized)
        return;

    // All the calibration registers are in order of our struct and are 2 bytes long too,
    // so just read them directly into our calib data
    readRegister(BMP280_REG_CALIB_COEFFS_START, (uint8_t *)&m_calib, sizeof(m_calib));

    // configure IIR filter, 0.5ms standby time (freerunning)
    constexpr uint8_t BMP280_FILTER = FILTER_COEFF << 2;
    writeRegister(BMP280_REG_CONFIG, (uint8_t *)&BMP280_FILTER, sizeof(BMP280_FILTER));

    m_initialized = true;
}

uint8_t BMP280::getTemperatureDuration()
{
    // Calculate in microseconds, then divide by 1000 for ms, rounding up
    constexpr uint32_t duration = 1250U +
        (2300U * ((1U << OVERSAMPLING_TEMPERATURE) >> 1)) +
        (2300U * ((1U << OVERSAMPLING_PRESSURE) >> 1) + 575U);
    return (FREERUNNING_NUM_SAMPLES * duration + 999U) / 1000U;
}

void BMP280::startTemperature()
{
    // Measure both Pressure and Temperature, freerunning to allow internal IIR to smooth extra samples for us
    constexpr uint8_t SAMPLING_MODE = (FREERUNNING_NUM_SAMPLES != 0) ? BMP280_MODE_NORMAL : BMP280_MODE_FORCED;
    constexpr uint8_t BMP280_MODE = (OVERSAMPLING_PRESSURE << 2 | OVERSAMPLING_TEMPERATURE << 5 | SAMPLING_MODE);
    writeRegister(BMP280_REG_CTRL_MEAS, (uint8_t *)&BMP280_MODE, sizeof(BMP280_MODE));
}

uint32_t BMP280::getPressure()
{
    return m_pressureLast;
}

int32_t BMP280::getTemperature()
{
    // uint8_t status = 0xff;
    // readRegister(BMP280_REG_STATUS, &status, sizeof(status));
    // if (status & bit(3))
    // {
    //     DBGLN("not ready");
    // }

    uint8_t buf[BMP280_LEN_TEMP_PRESS_DATA] = {0};
    readRegister(BMP280_REG_PRESSURE_MSB, buf, sizeof(buf));

    int32_t adc_P = ((uint32_t)buf[0] << 12) | ((uint32_t)buf[1] << 4) | ((uint32_t)buf[2] >> 4);
    int32_t adc_T = ((uint32_t)buf[3] << 12) | ((uint32_t)buf[4] << 4) | ((uint32_t)buf[5] >> 4);

    // BME280_compensate_T_int32()
    int32_t var01, var02;
    var01 = ((((adc_T >> 3) - ((int32_t)m_calib.dig_T1 << 1))) * ((int32_t)m_calib.dig_T2)) >> 11;
    var02  = (((((adc_T >> 4) - ((int32_t)m_calib.dig_T1)) * ((adc_T >> 4) - ((int32_t)m_calib.dig_T1))) >> 12) * ((int32_t)m_calib.dig_T3)) >> 14;
    int32_t t_fine = var01 + var02;

    // BME280_compensate_P_int64()
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)m_calib.dig_P6;
    var2 = var2 + ((var1*(int64_t)m_calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)m_calib.dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)m_calib.dig_P3) >> 8) + ((var1 * (int64_t)m_calib.dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)m_calib.dig_P1) >> 33;
    if (var1 == 0)
        return BaroBase::PRESSURE_INVALID;

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)m_calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)m_calib.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)m_calib.dig_P7) << 4);
    // p is pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits).
    // Output value of "24674867" represents 24674867/256 = 96386.2 Pa = 963.862 hPa
    m_pressureLast = ((uint32_t)p) >> 8;

    int32_t temperature = (t_fine * 5 + 128) >> 8;
    //DBGLN("%u t=%d p=%u", millis(), temperature, m_pressureLast);

    return temperature;
}

bool BMP280::detect()
{
    // Assumes Wire.begin() has already been called
    uint8_t chipid = 0;
    readRegister(BMP280_REG_CHIPID, &chipid, sizeof(chipid));
    return (chipid == BMP280_CHIPID || chipid == BME280_CHIPID);
}

#include <Wire.h>

#include "baro_spl06.h"
#include "baro_spl06_regs.h"

void SPL06::writeRegister(uint8_t reg, uint8_t *data, uint8_t size)
{
    Wire.beginTransmission(SPL06_I2C_ADDR);
    Wire.write(reg);
    Wire.write(data, size);
    Wire.endTransmission();
}

void SPL06::readRegister(uint8_t reg, uint8_t *data, uint8_t size)
{
    Wire.beginTransmission(SPL06_I2C_ADDR);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom((uint8_t)SPL06_I2C_ADDR, size);
    Wire.readBytes(data, size);
}

uint8_t SPL06::oversampleToRegVal(const uint8_t oversamples) const
{
    switch (oversamples)
    {
        default: // fallthrough
        case 1: return 0;
        case 2: return 1;
        case 4: return 2;
        case 8: return 3;
        case 16: return 4;
        case 32: return 5;
        case 64: return 6;
        case 128: return 7;
    }
}

int32_t SPL06::oversampleToScaleFactor(const uint8_t oversamples) const
{
    switch (oversamples)
    {
        default: // falthrough
        case 1: return 524288;
        case 2: return 1572864;
        case 4: return 3670016;
        case 8: return 7864320;
        case 16: return 253952;
        case 32: return 516096;
        case 64: return 1040384;
        case 128: return 2088960;
    }
}

void SPL06::initialize()
{
    if (m_initialized)
        return;

    // Step 1: Load calibration data
    uint8_t status = 0;
    readRegister(SPL06_MODE_AND_STATUS_REG, &status, sizeof(status));
    if (((status & SPL06_MEAS_CFG_COEFFS_RDY) == 0) || ((status & SPL06_MEAS_CFG_SENSOR_RDY) == 0))
        return;

    uint8_t caldata[SPL06_CALIB_COEFFS_LEN];
    readRegister(SPL06_CALIB_COEFFS_START, caldata, sizeof(caldata));

    m_calib.c0 = (caldata[0] & 0x80 ? 0xF000 : 0) | ((uint16_t)caldata[0] << 4) | (((uint16_t)caldata[1] & 0xF0) >> 4);
    m_calib.c1 = ((caldata[1] & 0x8 ? 0xF000 : 0) | ((uint16_t)caldata[1] & 0x0F) << 8) | (uint16_t)caldata[2];
    m_calib.c00 = (caldata[3] & 0x80 ? 0xFFF00000 : 0) | ((uint32_t)caldata[3] << 12) | ((uint32_t)caldata[4] << 4) | (((uint32_t)caldata[5] & 0xF0) >> 4);
    m_calib.c10 = (caldata[5] & 0x8 ? 0xFFF00000 : 0) | (((uint32_t)caldata[5] & 0x0F) << 16) | ((uint32_t)caldata[6] << 8) | (uint32_t)caldata[7];
    m_calib.c01 = ((uint16_t)caldata[8] << 8) | ((uint16_t)caldata[9]);
    m_calib.c11 = ((uint16_t)caldata[10] << 8) | (uint16_t)caldata[11];
    m_calib.c20 = ((uint16_t)caldata[12] << 8) | (uint16_t)caldata[13];
    m_calib.c21 = ((uint16_t)caldata[14] << 8) | (uint16_t)caldata[15];
    m_calib.c30 = ((uint16_t)caldata[16] << 8) | (uint16_t)caldata[17];

    // Step 2: Set up oversampling and FIFO
    uint8_t reg_value;
    reg_value = SPL06_TEMP_USE_EXT_SENSOR | oversampleToRegVal(OVERSAMPLING_TEMPERATURE);
    writeRegister(SPL06_TEMPERATURE_CFG_REG, &reg_value, sizeof(reg_value));

    reg_value = oversampleToRegVal(OVERSAMPLING_PRESSURE);
    writeRegister(SPL06_PRESSURE_CFG_REG, &reg_value, sizeof(reg_value));

    reg_value = 0;
    if (OVERSAMPLING_TEMPERATURE > 8)
        reg_value |= SPL06_TEMPERATURE_RESULT_BIT_SHIFT;
    if (OVERSAMPLING_PRESSURE > 8)
        reg_value |= SPL06_PRESSURE_RESULT_BIT_SHIFT;
    writeRegister(SPL06_INT_AND_FIFO_CFG_REG, &reg_value, sizeof(reg_value));

    // Done!
    m_initialized = true;
}

uint8_t SPL06::getTemperatureDuration()
{
    return SPL06_MEASUREMENT_TIME(OVERSAMPLING_TEMPERATURE);
}

void SPL06::startTemperature()
{
    uint8_t reg_value = SPL06_MEAS_TEMPERATURE;
    writeRegister(SPL06_MODE_AND_STATUS_REG, &reg_value, sizeof(reg_value));
}

int32_t SPL06::getTemperature()
{
    uint8_t status;
    readRegister(SPL06_MODE_AND_STATUS_REG, &status, sizeof(status));
    if ((status & SPL06_MEAS_CFG_TEMPERATURE_RDY) == 0)
        return TEMPERATURE_INVALID;

    uint8_t data[SPL06_TEMPERATURE_LEN];
    readRegister(SPL06_TEMPERATURE_START_REG, data, sizeof(data));

    // Unpack and descale
    int32_t uncorr_temp = (int32_t)((data[0] & 0x80 ? 0xFF000000 : 0) | (((uint32_t)(data[0])) << 16) | (((uint32_t)(data[1])) << 8) | ((uint32_t)data[2]));
    m_temperatureLast = (float)uncorr_temp / oversampleToScaleFactor(OVERSAMPLING_TEMPERATURE);

    // Adjust for calibration
    const float temp_comp = (float)m_calib.c0 / 2 + m_temperatureLast * m_calib.c1;

    return temp_comp * 100;
}

uint8_t SPL06::getPressureDuration()
{
    return SPL06_MEASUREMENT_TIME(OVERSAMPLING_PRESSURE);
}

void SPL06::startPressure()
{
    uint8_t reg_value = SPL06_MEAS_PRESSURE;
    writeRegister(SPL06_MODE_AND_STATUS_REG, &reg_value, sizeof(reg_value));
}

uint32_t SPL06::getPressure()
{
    uint8_t status;
    readRegister(SPL06_MODE_AND_STATUS_REG, &status, sizeof(status));
    if ((status & SPL06_MEAS_CFG_PRESSURE_RDY) == 0)
        return PRESSURE_INVALID;

    uint8_t data[SPL06_PRESSURE_LEN];
    readRegister(SPL06_PRESSURE_START_REG, data, sizeof(data));

    // Unpack and descale
    int32_t uncorr_press = (int32_t)((data[0] & 0x80 ? 0xFF000000 : 0) | (((uint32_t)(data[0])) << 16) | (((uint32_t)(data[1])) << 8) | ((uint32_t)data[2]));
    const float p_raw_sc = (float)uncorr_press / oversampleToScaleFactor(OVERSAMPLING_PRESSURE);

    // Adjust for calibration and temperature
    const float pressure_cal = (float)m_calib.c00 + p_raw_sc * ((float)m_calib.c10 + p_raw_sc * ((float)m_calib.c20 + p_raw_sc * m_calib.c30));
    const float p_temp_comp = m_temperatureLast * ((float)m_calib.c01 + p_raw_sc * ((float)m_calib.c11 + p_raw_sc * m_calib.c21));

    return (pressure_cal + p_temp_comp) * 10.0;
}

bool SPL06::detect()
{
    // Assumes Wire.begin() has already been called
    uint8_t chipid = 0;
    readRegister(SPL06_CHIP_ID_REG, &chipid, sizeof(chipid));
    return chipid == SPL06_DEFAULT_CHIP_ID;
}



#include "baro_spa06.h"
#include "baro_spa06_regs.h"

// Oversampling settings and scale factors for SPA06-003 command mode.
static constexpr uint8_t PRESSURE_CFG = 4; // 16x
static constexpr float PRESSURE_SCALE = 253952.0f;
static constexpr uint8_t TEMPERATURE_CFG = 0; // 1x
static constexpr float TEMPERATURE_SCALE = 524288.0f;

static int32_t signExtend(int32_t value, uint8_t bits)
{
    const int32_t signBit = int32_t{1} << (bits - 1);
    return (value ^ signBit) - signBit;
}

static bool isStandby(uint8_t status)
{
    return (status & SPA06_MEAS_CFG_SENSOR_RDY) && (status & SPA06_MEAS_MODE_MASK) == 0;
}

void SPA06::initialize()
{
    if (m_initialized)
        return;

    // Reset once and let the next Baro_Init call check sensor readiness.
    if (!m_resetIssued)
    {
        uint8_t reset = SPA06_SOFT_RESET;
        writeRegister(SPA06_RESET_REG, &reset, sizeof(reset));
        m_resetIssued = true;
        return;
    }

    constexpr uint8_t ready = SPA06_MEAS_CFG_SENSOR_RDY | SPA06_MEAS_CFG_COEFFS_RDY;
    uint8_t status = 0;
    readRegister(SPA06_MODE_AND_STATUS_REG, &status, sizeof(status));
    if ((status & ready) != ready)
        return;

    uint8_t caldata[SPA06_CALIB_COEFFS_LEN] = {};
    readRegister(SPA06_CALIB_COEFFS_START, caldata, sizeof(caldata));

    m_calib.c0 = signExtend((caldata[0] << 4) | (caldata[1] >> 4), 12);
    m_calib.c1 = signExtend(((caldata[1] & 0x0F) << 8) | caldata[2], 12);
    m_calib.c00 = signExtend((caldata[3] << 12) | (caldata[4] << 4) | (caldata[5] >> 4), 20);
    m_calib.c10 = signExtend(((caldata[5] & 0x0F) << 16) | (caldata[6] << 8) | caldata[7], 20);
    m_calib.c01 = signExtend((caldata[8] << 8) | caldata[9], 16);
    m_calib.c11 = signExtend((caldata[10] << 8) | caldata[11], 16);
    m_calib.c20 = signExtend((caldata[12] << 8) | caldata[13], 16);
    m_calib.c21 = signExtend((caldata[14] << 8) | caldata[15], 16);
    m_calib.c30 = signExtend((caldata[16] << 8) | caldata[17], 16);
    m_calib.c31 = signExtend((caldata[18] << 4) | (caldata[19] >> 4), 12);
    m_calib.c40 = signExtend(((caldata[19] & 0x0F) << 8) | caldata[20], 12);

    uint8_t reg_value = TEMPERATURE_CFG;
    writeRegister(SPA06_TEMPERATURE_CFG_REG, &reg_value, sizeof(reg_value));

    reg_value = PRESSURE_CFG;
    writeRegister(SPA06_PRESSURE_CFG_REG, &reg_value, sizeof(reg_value));

    reg_value = SPA06_PRESSURE_RESULT_BIT_SHIFT;
    writeRegister(SPA06_INT_AND_FIFO_CFG_REG, &reg_value, sizeof(reg_value));

    m_initialized = true;
}

uint8_t SPA06::getTemperatureDuration()
{
    return 4; // 3.6 ms at 1x oversampling
}

void SPA06::startTemperature()
{
    // Leave TMP_EXT clear to use the internal temperature sensor.
    uint8_t reg_value = SPA06_MEAS_TEMPERATURE;
    writeRegister(SPA06_MODE_AND_STATUS_REG, &reg_value, sizeof(reg_value));
}

int32_t SPA06::getTemperature()
{
    uint8_t status = 0;
    readRegister(SPA06_MODE_AND_STATUS_REG, &status, sizeof(status));
    if ((status & SPA06_MEAS_CFG_TEMPERATURE_RDY) == 0)
    {
        // Retry a missed command once the sensor reports standby.
        if (isStandby(status))
            startTemperature();
        return TEMPERATURE_INVALID;
    }

    uint8_t data[3] = {};
    readRegister(SPA06_TEMPERATURE_START_REG, data, sizeof(data));
    const int32_t raw = signExtend((data[0] << 16) | (data[1] << 8) | data[2], 24);

    m_temperatureLast = raw / TEMPERATURE_SCALE;
    const float temperature = m_calib.c0 * 0.5f + m_temperatureLast * m_calib.c1;
    return temperature * 100;
}

uint8_t SPA06::getPressureDuration()
{
    return 35; // Maximum at 16x oversampling
}

void SPA06::startPressure()
{
    uint8_t reg_value = SPA06_MEAS_PRESSURE;
    writeRegister(SPA06_MODE_AND_STATUS_REG, &reg_value, sizeof(reg_value));
}

uint32_t SPA06::getPressure()
{
    uint8_t status = 0;
    readRegister(SPA06_MODE_AND_STATUS_REG, &status, sizeof(status));
    if ((status & SPA06_MEAS_CFG_PRESSURE_RDY) == 0)
    {
        if (isStandby(status))
            startPressure();
        return PRESSURE_INVALID;
    }

    uint8_t data[3] = {};
    readRegister(SPA06_PRESSURE_START_REG, data, sizeof(data));
    const int32_t raw = signExtend((data[0] << 16) | (data[1] << 8) | data[2], 24);

    // SPA06 adds the c40 pressure term and the c31 temperature compensation term.
    const float p = raw / PRESSURE_SCALE;
    const float pressureCal = m_calib.c00 + p * (m_calib.c10 + p * (m_calib.c20 + p * (m_calib.c30 + p * m_calib.c40)));
    const float tempComp = m_temperatureLast * (m_calib.c01 + p * (m_calib.c11 + p * (m_calib.c21 + p * m_calib.c31)));
    const float pressure = (pressureCal + tempComp) * 10.0f;
    if (pressure <= 0 || pressure >= PRESSURE_INVALID)
        return PRESSURE_INVALID;

    return pressure;
}

bool SPA06::detect()
{
    // Assumes Wire.begin() has already been called.
    const uint8_t addresses[] = { SPA06_I2C_ADDR, SPA06_I2C_ADDR_ALT };
    for (uint8_t address : addresses)
    {
        m_address = address;
        uint8_t chipid = 0;
        readRegister(SPA06_CHIP_ID_REG, &chipid, sizeof(chipid));
        if (chipid == SPA06_DEFAULT_CHIP_ID)
            return true;
    }

    return false;
}

#include "mpu9250.h"
#include <Wire.h>
#include "logging.h"

// ---- MPU9250 registers ----
#define REG_SMPLRT_DIV      0x19
#define REG_CONFIG           0x1A
#define REG_GYRO_CONFIG       0x1B
#define REG_ACCEL_CONFIG      0x1C
#define REG_INT_PIN_CFG        0x37
#define REG_USER_CTRL           0x6A
#define REG_PWR_MGMT_1            0x6B
#define REG_WHO_AM_I               0x75
#define REG_ACCEL_XOUT_H             0x3B

#define WHOAMI_MPU9250       0x71
#define WHOAMI_MPU9255       0x73

// ---- AK8963 (magnetometer) registers, accessed via bypass ----
#define AK_REG_WIA           0x00
#define AK_REG_ST1             0x02
#define AK_REG_HXL               0x03
#define AK_REG_CNTL1                0x0A
#define AK_WHOAMI             0x48

bool MPU9250::detect(uint8_t *outAddress)
{
    return detect(outAddress, nullptr, nullptr, nullptr, nullptr);
}

bool MPU9250::detect(uint8_t *outAddress, uint8_t *outErr68, uint8_t *outWhoami68, uint8_t *outErr69, uint8_t *outWhoami69)
{
    Wire.setClock(100000);
    uint8_t err68 = 99, whoami68 = 0;
    uint8_t err69 = 99, whoami69 = 0;

    // Probe 0x68
    Wire.beginTransmission(0x68);
    err68 = Wire.endTransmission(true);
    if (err68 == 0)
    {
        Wire.beginTransmission(0x68);
        Wire.write(REG_WHO_AM_I);
        if (Wire.endTransmission(true) == 0)
        {
            if (Wire.requestFrom((int)0x68, 1) == 1)
            {
                whoami68 = Wire.read();
            }
        }
    }

    // Probe 0x69
    Wire.beginTransmission(0x69);
    err69 = Wire.endTransmission(true);
    if (err69 == 0)
    {
        Wire.beginTransmission(0x69);
        Wire.write(REG_WHO_AM_I);
        if (Wire.endTransmission(true) == 0)
        {
            if (Wire.requestFrom((int)0x69, 1) == 1)
            {
                whoami69 = Wire.read();
            }
        }
    }

    if (outErr68) *outErr68 = err68;
    if (outWhoami68) *outWhoami68 = whoami68;
    if (outErr69) *outErr69 = err69;
    if (outWhoami69) *outWhoami69 = whoami69;

    auto isValidWhoami = [](uint8_t w) {
        return (w == WHOAMI_MPU9250 || w == WHOAMI_MPU9255 || w == 0x68 || w == 0x70 || w == 0x72 || w == 0x98 || w == 0x74 || w == 0x75 || w == 0x11 || w == 0x12 || w == 0xAF);
    };

    if (isValidWhoami(whoami68))
    {
        if (outAddress) *outAddress = 0x68;
        return true;
    }
    if (isValidWhoami(whoami69))
    {
        if (outAddress) *outAddress = 0x69;
        return true;
    }
    return false;
}

bool MPU9250::detect(uint8_t address)
{
    Wire.setClock(100000);
    Wire.beginTransmission(address);
    if (Wire.endTransmission(true) != 0)
        return false;

    Wire.beginTransmission(address);
    Wire.write(REG_WHO_AM_I);
    if (Wire.endTransmission(true) != 0)
        return false;
    if (Wire.requestFrom((int)address, 1) != 1)
        return false;
    const uint8_t whoami = Wire.read();
    return (whoami == WHOAMI_MPU9250 || whoami == WHOAMI_MPU9255 || whoami == 0x68 || whoami == 0x70 || whoami == 0x72 || whoami == 0x98);
}

void MPU9250::writeRegAt(uint8_t i2cAddr, uint8_t reg, uint8_t value) const
{
    Wire.beginTransmission(i2cAddr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission(true);
}

bool MPU9250::readRegsAt(uint8_t i2cAddr, uint8_t reg, uint8_t *buf, uint8_t len) const
{
    Wire.beginTransmission(i2cAddr);
    Wire.write(reg);
    if (Wire.endTransmission(true) != 0)
        return false;
    if (Wire.requestFrom((int)i2cAddr, (int)len) != len)
        return false;
    for (uint8_t i = 0; i < len; i++)
        buf[i] = Wire.read();
    return true;
}

void MPU9250::writeReg(uint8_t reg, uint8_t value) const { writeRegAt(m_address, reg, value); }
uint8_t MPU9250::readReg(uint8_t reg) const { uint8_t v = 0; readRegsAt(m_address, reg, &v, 1); return v; }
bool MPU9250::readRegs(uint8_t reg, uint8_t *buf, uint8_t len) const { return readRegsAt(m_address, reg, buf, len); }

bool MPU9250::initialize(uint8_t address, bool enableMag)
{
    m_address = address;
    Wire.setClock(100000);

    // Wake the chip from default sleep mode (PWR_MGMT_1 bit 6 = 1 on power-up)
    writeReg(REG_PWR_MGMT_1, 0x00);
    delay(15);
    // Select best available clock source (PLL with X gyro)
    writeReg(REG_PWR_MGMT_1, 0x01);
    delay(15);

    // Disable the internal I2C-master (we don't need it) so bypass mode is usable.
    writeReg(REG_USER_CTRL, 0x00);

    // 1kHz internal sample rate, no extra divider.
    writeReg(REG_SMPLRT_DIV, 0x00);
    // DLPF: ~184Hz bandwidth, decent noise/latency tradeoff for telemetry use.
    writeReg(REG_CONFIG, 0x01);
    // Gyro full scale +-250 dps (default, most sensitive).
    writeReg(REG_GYRO_CONFIG, 0x00);
    // Accel full scale +-2g (default, most sensitive).
    writeReg(REG_ACCEL_CONFIG, 0x00);

    uint8_t whoami = readReg(REG_WHO_AM_I);
    m_initialized = (whoami == WHOAMI_MPU9250 || whoami == WHOAMI_MPU9255 || whoami == 0x68 || whoami == 0x70 || whoami == 0x72 || whoami == 0x98);
    DBGLN("IMU initialized status=%d, whoami=0x%02X at addr=0x%02X", m_initialized, whoami, m_address);

    if (m_initialized && enableMag)
    {
        // Open the bypass mux so the AK8963 is directly addressable on the bus.
        writeReg(REG_INT_PIN_CFG, 0x02);
        delay(10);

        uint8_t akWhoami = 0;
        if (readRegsAt(AK8963_ADDR, AK_REG_WIA, &akWhoami, 1) && akWhoami == AK_WHOAMI)
        {
            // Continuous measurement mode 2 (100Hz), 16-bit output.
            writeRegAt(AK8963_ADDR, AK_REG_CNTL1, 0x16);
            delay(10);
            m_magEnabled = true;
            DBGLN("AK8963 Magnetometer enabled! WHO_AM_I=0x%02X", akWhoami);
        }
        else
        {
            DBGLN("AK8963 Mag not found (whoami=0x%02X)", akWhoami);
        }
    }

    if (m_initialized)
    {
        Wire.setClock(400000);
    }
    return m_initialized;
}

bool MPU9250::readAccelGyro(int16_t &ax, int16_t &ay, int16_t &az,
                             int16_t &gx, int16_t &gy, int16_t &gz)
{
    uint8_t raw[14];
    if (!readRegs(REG_ACCEL_XOUT_H, raw, sizeof(raw)))
        return false;

    ax = (int16_t)((raw[0] << 8) | raw[1]);
    ay = (int16_t)((raw[2] << 8) | raw[3]);
    az = (int16_t)((raw[4] << 8) | raw[5]);
    // raw[6..7] is temperature, unused here.
    gx = (int16_t)((raw[8] << 8) | raw[9]);
    gy = (int16_t)((raw[10] << 8) | raw[11]);
    gz = (int16_t)((raw[12] << 8) | raw[13]);
    return true;
}

bool MPU9250::readMag(int16_t &mx, int16_t &my, int16_t &mz)
{
    if (!m_magEnabled)
        return false;

    uint8_t st1 = 0;
    if (!readRegsAt(AK8963_ADDR, AK_REG_ST1, &st1, 1) || !(st1 & 0x01))
        return false; // no fresh sample yet

    uint8_t raw[7]; // HXL,HXH,HYL,HYH,HZL,HZH,ST2 -- ST2 must be read to latch the registers
    if (!readRegsAt(AK8963_ADDR, AK_REG_HXL, raw, sizeof(raw)))
        return false;

    // AK8963 is little-endian, unlike the MPU registers.
    mx = (int16_t)((raw[1] << 8) | raw[0]);
    my = (int16_t)((raw[3] << 8) | raw[2]);
    mz = (int16_t)((raw[5] << 8) | raw[4]);
    return true;
}

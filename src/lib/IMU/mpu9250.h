#pragma once

#include <stdint.h>

// MPU9250 default I2C address (AD0 pin low). Some breakouts pull AD0 high -> 0x69.
#define MPU9250_ADDR        0x68

// AK8963 magnetometer lives behind the MPU's I2C bypass at a fixed address.
#define AK8963_ADDR         0x0C

class MPU9250
{
public:
    // Probes the bus for a WHO_AM_I match. Call before initialize().
    // If outAddress is provided, writes the detected I2C address (0x68 or 0x69).
    static bool detect(uint8_t *outAddress = nullptr);
    static bool detect(uint8_t *outAddress, uint8_t *outErr68, uint8_t *outWhoami68, uint8_t *outErr69, uint8_t *outWhoami69);
    static bool detect(uint8_t address);

    // Wakes the chip, sets full-scale ranges, and (if enableMag) opens the
    // I2C bypass so the AK8963 magnetometer is reachable directly and puts
    // it into continuous-measurement mode.
    bool initialize(uint8_t address = MPU9250_ADDR, bool enableMag = true);

    bool isInitialized() const { return m_initialized; }
    uint8_t getAddress() const { return m_address; }

    // Burst-reads accel+temp+gyro in one transaction (14 bytes from 0x3B).
    // Returns false on I2C failure; values are left unchanged in that case.
    bool readAccelGyro(int16_t &ax, int16_t &ay, int16_t &az,
                        int16_t &gx, int16_t &gy, int16_t &gz);

    // Reads magnetometer if enabled and a fresh sample is available (ST1 DRDY bit).
    // Returns false if no new sample was ready or mag is disabled -- this is
    // normal since the AK8963 samples slower (~100Hz) than the accel/gyro.
    bool readMag(int16_t &mx, int16_t &my, int16_t &mz);

    bool magEnabled() const { return m_magEnabled; }

private:
    void writeReg(uint8_t reg, uint8_t value) const;
    uint8_t readReg(uint8_t reg) const;
    bool readRegs(uint8_t reg, uint8_t *buf, uint8_t len) const;

    void writeRegAt(uint8_t i2cAddr, uint8_t reg, uint8_t value) const;
    bool readRegsAt(uint8_t i2cAddr, uint8_t reg, uint8_t *buf, uint8_t len) const;

    uint8_t m_address = MPU9250_ADDR;
    bool m_initialized = false;
    bool m_magEnabled = false;
};

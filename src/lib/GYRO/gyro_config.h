#pragma once

#if defined(GYRO_SUPPORT) && defined(PLATFORM_ESP32)
#include "gyro_types.h"
#include <nvs.h>

#define GYRO_CONFIG_VERSION 8U

///////////////////////////////////////////////////

class GyroConfig
{
public:
    GyroConfig();
    ~GyroConfig() = default;

    void Load();
    uint32_t Commit();
    bool IsModified() const { return m_modified != 0; }

    // Getters
    // const bool GetPwmChannelInverted(uint8_t ch) const { return m_config.pwmChannels[ch].val.inverted; }
    const rx_config_pwm_limits_t *GetPwmChannelLimits(uint8_t ch) const { return &m_config.pwmLimits[ch]; }

    const rx_config_gyro_channel_t *GetGyroChannel(uint8_t ch) const { return &m_config.gyroChannels[ch]; }
    // const rx_config_gyro_timings_t *GetGyroChannelTimings(uint8_t ch) const { return &m_config.gyroTimings[ch]; }
    const rx_config_gyro_PID_t *GetGyroPID(gyro_pidgroup_t group, gyro_axis_t axis) const { return &m_config.gyroPIDs[group][axis]; }
    const rx_config_gyro_fmode_t *GetGyroFMode(gyro_mode_t fm) const { return (fm >= GYRO_MODE_RATE) ? &m_config.gyroFModes[fm - GYRO_MODE_RATE] : nullptr; }

    const rx_config_gyro_mode_pos_t *GetGyroModePos() const { return &m_config.gyroModeSwitch; }
    const uint8_t GetGyroOrientationH() const { return m_config.global.val.orientationH; }
    const uint8_t GetGyroOrientationV() const { return m_config.global.val.orientationV; }
    const bool GetGyroEnabled() const { return m_config.global.val.gyroEnabled; }
    const uint8_t GetGyroConfigVersion() const { return m_config.configVersion; }
    const gyro_gain_factor_t GetGyroGainFactor() const { return (gyro_gain_factor_t)m_config.global.val.gainFactor; };
    const rx_config_gyro_calibration_t *GetAccelCalibration() const { return &m_config.accelCalibration; }
    const rx_config_gyro_calibration_t *GetGyroCalibration() const { return &m_config.gyroCalibration; }

    // Setters
    void SetDefaults(bool commit);

    void SetGyroEnabled(bool);
    void SetGyroGainFactor(gyro_gain_factor_t factor);
    void SetAccelCalibration(uint16_t x, uint16_t y, uint16_t z);
    void SetGyroCalibration(uint16_t x, uint16_t y, uint16_t z);
    void SetGyroOrientation(uint8_t oh, uint8_t ov);

    void SetPwmChannelLimits(uint8_t ch, uint16_t min, uint16_t max, uint16_t mid);
    void SetPwmChannelLimitsRaw(uint8_t ch, uint64_t raw);

    void SetGyroChannel(uint8_t ch, uint8_t output_mode, bool master, bool inverted);
    void SetGyroChannelRaw(uint8_t ch, uint32_t raw);
    void SetGyroFModeRaw(gyro_mode_t fm, uint64_t raw);
    void SetGyroModePos(uint8_t pos, gyro_mode_t mode);
    void SetGyroPIDRate(gyro_pidgroup_t group, gyro_axis_t axis, gyro_rate_variable_t var, uint8_t value);

private:
    void debugGyroConfiguration();

    nvs_handle handle;
    rx_config_gyro_t m_config{};
    uint32_t m_modified = 0;
};

#endif

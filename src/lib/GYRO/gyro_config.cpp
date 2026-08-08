#if defined(GYRO_SUPPORT) && defined(PLATFORM_ESP32)
#include "common.h"
#include "device.h"
#include "helpers.h"
#include "logging.h"
#include "gyro_config.h"
#include "gyro.h"
#include <nvs_flash.h>

GyroConfig::GyroConfig()
{
}

void GyroConfig::Load()
{
    DBGLN("Gyro Config Load");

    m_modified = 0;

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );
    ESP_ERROR_CHECK(nvs_open("GYRO", NVS_READWRITE, &handle));

    // Try to load the version and make sure it is a Gyro config
    uint32_t version = 0;
    nvs_get_u32(handle, "version", &version);
    DBGLN("Gyro Config: version %u", version);

    // Can't downgrade when flashing a previous version, just use defaults.
    if (version > GYRO_CONFIG_VERSION)
    {
        SetDefaults(true);
        return;
    }

    uint32_t value32;
    uint64_t value64;

     #define LOAD(get, key, tmp, newValue ) \
        { if (get(handle, key, & tmp) == ESP_OK) { newValue = tmp; } }

    #define LOAD_32(key,newValue) LOAD(nvs_get_u32, key, value32, newValue)
    #define LOAD_64(key,newValue) LOAD(nvs_get_u64, key, value64, newValue)

    LOAD_32("global", m_config.global.raw);
    LOAD_64("cal_accel", m_config.accelCalibration.raw);
    LOAD_64("cal_gyro", m_config.gyroCalibration.raw);
    LOAD_32("mode_sw", m_config.gyroModeSwitch.raw);

    for (int i=0 ; i < PWM_MAX_CHANNELS ; i++)
    {
        char pwm[10] = "pwm_";
        itoa(i, pwm+4, 10);
        LOAD_64(pwm, m_config.pwmLimits[i].raw);
    }

    for (int i=0 ; i < GYRO_MAX_CHANNELS ; i++)
    {
        char gyro_ch[10] = "ch_";
        itoa(i, gyro_ch+3, 10);
        LOAD_32(gyro_ch, m_config.gyroChannels[i].raw);
    }

    for (int i=0 ; i < GYRO_PID_GROUP_MAX ; i++)
    {
        char pid[16] = "pid_";
        itoa(i, pid+4, 10);
        for (int j=0 ; j < GYRO_N_AXES ; j++)
        {
            strcat(pid, "_");
            itoa(i, pid+strlen(pid), 10);
            LOAD_32(pid, m_config.gyroPIDs[i][j].raw);
        }        
    }

    for (int i=0 ; i < GYRO_MODE_MAX ; i++)
    {
        char fmode[10] = "fmode_";
        itoa(i, fmode+6, 10);
        LOAD_64(fmode, m_config.gyroFModes[i].raw);
    }

    if (version != GYRO_CONFIG_VERSION)
    {
        m_modified |= EVENT_CONFIG_VERSION_CHANGED;
    }
}

uint32_t
GyroConfig::Commit()
{
    DBGLN("Gyro Config Commit");

    int changes = 0;
    uint32_t value32;
    uint64_t value64;

    #define SET_IF_CHAGED(get, set, key, oldValue, newValue ) \
        { get(handle, key, & oldValue); if (oldValue != newValue) { set(handle, key, newValue); changes++; } }

    #define SET_IF_CHAGED_32(key,newValue) SET_IF_CHAGED(nvs_get_u32, nvs_set_u32, key, value32, newValue)
    #define SET_IF_CHAGED_64(key,newValue) SET_IF_CHAGED(nvs_get_u64, nvs_set_u64, key, value64, newValue)
   
    if (!m_modified)
    {
        DBGLN("No changes");
        return 0;
    }

    SET_IF_CHAGED_32("version", m_config.configVersion);
    SET_IF_CHAGED_32("global", m_config.global.raw);
    SET_IF_CHAGED_64("cal_accel", m_config.accelCalibration.raw);
    SET_IF_CHAGED_64("cal_gyro", m_config.gyroCalibration.raw);
    SET_IF_CHAGED_32("mode_sw", m_config.gyroModeSwitch.raw);

    for (int i=0 ; i < PWM_MAX_CHANNELS ; i++)
    {
        char pwm[10] = "pwm_";
        itoa(i, pwm+4, 10);
        SET_IF_CHAGED_64(pwm, m_config.pwmLimits[i].raw);
    }
    
    for (int i=0 ; i < GYRO_MAX_CHANNELS ; i++)
    {
        char gyro_ch[10] = "ch_";
        itoa(i, gyro_ch+3, 10);
        SET_IF_CHAGED_32(gyro_ch, m_config.gyroChannels[i].raw);
    }

    for (int i=0 ; i < GYRO_PID_GROUP_MAX ; i++)
    {
        char pid[16] = "pid_";
        itoa(i, pid+4, 10);
        for (int j=0 ; j<GYRO_N_AXES ; j++)
        {
            strcat(pid, "_");
            itoa(i, pid+strlen(pid), 10);
            SET_IF_CHAGED_32(pid,m_config.gyroPIDs[i][j].raw);
        }
    }

    for (int i=0 ; i < GYRO_MODE_MAX ; i++)
    {
        char fmode[10] = "fmode_";
        itoa(i, fmode+6, 10);
        SET_IF_CHAGED_64(fmode, m_config.gyroFModes[i].raw);
    }

    if (changes > 0) {
        DBGLN("Saved %d changes.",changes);
    }

    nvs_commit(handle);
    changes = m_modified;
    m_modified = 0;
    return changes;
}

/////////////////////////////////////////////////////

void GyroConfig::SetDefaults(bool commit)
{
    memset(&m_config, 0, sizeof(m_config));

    SetGyroConfigVersion(GYRO_CONFIG_VERSION);
    SetGyroEnabled(false);
    SetGyroOrientation(6, 6); // 6=No orientation Set
    SetGyroGainFactor(GYRO_GAIN_FACTOR_1X);

    // Configure Limits
    for (unsigned int ch = 0; ch < PWM_MAX_CHANNELS; ch++)
    {
        SetPwmChannelLimits(ch, GYRO_US_MIN, GYRO_US_MAX, GYRO_US_MID);
    }

    // Configure PIDS
    for (int g = 0; g <= GYRO_PID_GROUP_LAST_ACTIVE; g++)
    {
        auto group = (gyro_pidgroup_t)g;
        for (int i = 0; i < 3; i++)
        {
            auto axis = (gyro_axis_t)i;
            SetGyroPIDRate(group, axis, GYRO_RATE_VARIABLE_P, 35);
            SetGyroPIDRate(group, axis, GYRO_RATE_VARIABLE_I, 0);
            SetGyroPIDRate(group, axis, GYRO_RATE_VARIABLE_D, 10);
        }
    }

    // MADWICK PI
    SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_P, 20);
    SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_I, 00);
    SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_D, 00);

    // Configure channel Functions
    for (int ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
    {
        SetGyroChannel(ch, FN_NONE, false, false);
    }

    // Configure Gyro Switch
    for (int p = 0; p < 5; p++)
    {
        SetGyroModePos(p, GYRO_MODE_OFF);
    }
    SetGyroModePos(0, GYRO_MODE_OFF);
    SetGyroModePos(1, GYRO_MODE_OFF);
    SetGyroModePos(2, GYRO_MODE_RATE);
    SetGyroModePos(3, GYRO_MODE_OFF);
    SetGyroModePos(4, GYRO_MODE_LEVEL);

    for (int fm = GYRO_MODE_RATE; fm <= GYRO_MODE_LAST_ACTIVE; fm++)
    {
        // Skip Gyro OFF, 1 based
        rx_config_gyro_fmode_t tmp;
        tmp.raw = 0;

        tmp.val.useRate = 1;

        // Only Rate
        tmp.val.stickPri = STICK_PRIORITY_100;

        // For Auto-Level/Envelope
        tmp.val.maxAnglePitch = 40;
        tmp.val.maxAngleRoll = 70;

        // For Auto-Level/Launch
        tmp.val.trimPitch = (fm == GYRO_MODE_LAUNCH) ? 10 : 0;
        tmp.val.trimRoll = 0;

        // Gains for everybody
        tmp.val.gainRoll = (fm == GYRO_MODE_RATE) ? 30 : 35;
        tmp.val.gainPitch = (fm == GYRO_MODE_RATE) ? 40 : 35;
        tmp.val.gainYaw = (fm == GYRO_MODE_RATE) ? 50 : 35;

        SetGyroFModeRaw((gyro_mode_t)fm, tmp.raw);
    }
    
    if (commit)
    {
        m_modified = EVENT_CONFIG_GYRO_CHANGED;
        Commit();
    } else {
        m_modified = 0;
    }
}

void GyroConfig::debugGyroConfiguration()
{
    DBGLN("Gyro configuration:");
    for (uint8_t ch = 0; ch < PWM_MAX_CHANNELS; ch++)
    {
        rx_config_gyro_channel_t *config = &m_config.gyroChannels[ch];
        if (config->val.output_mode != FN_NONE)
            DBGLN("CH%d: Fun=%d, %s, %s", ch, config->val.output_mode,
                config->val.master ? "master" : "",
                config->val.inverted ? "inverted" : "");
    }
}

void GyroConfig::SetGyroChannel(uint8_t ch, uint8_t output_mode, bool master, bool inverted)
{
    if (ch > PWM_MAX_CHANNELS)
        return;

    rx_config_gyro_channel_t *config = &m_config.gyroChannels[ch];
    rx_config_gyro_channel_t newConfig;
    newConfig.val.output_mode = output_mode;

    newConfig.val.inverted = inverted;
    newConfig.val.master = master;

    if (config->raw == newConfig.raw)
        return;

    config->raw = newConfig.raw;
    debugGyroConfiguration();
    m_modified = EVENT_CONFIG_GYRO_CHANGED;
}

void GyroConfig::SetGyroChannelRaw(uint8_t ch, uint32_t raw)
{
    if (ch > GYRO_MAX_CHANNELS)
        return;

    rx_config_gyro_channel_t *config = &m_config.gyroChannels[ch];
    if (config->raw == raw)
        return;

    config->raw = raw;
    m_modified = EVENT_CONFIG_GYRO_CHANGED;
    debugGyroConfiguration();
}

void GyroConfig::SetGyroFModeRaw(gyro_mode_t fm, uint64_t raw)
{
    // Storage array is relative to GYRO_MODE_RATE (1)
    if (fm < GYRO_MODE_RATE || fm > GYRO_MODE_LAST_ACTIVE)
        return;

    rx_config_gyro_fmode_t *config = &m_config.gyroFModes[fm - GYRO_MODE_RATE];
    if (config->raw == raw)
        return;

    config->raw = raw;
    m_modified = EVENT_CONFIG_GYRO_CHANGED;
}

void GyroConfig::SetGyroModePos(uint8_t pos, gyro_mode_t mode)
{
    if (pos > 4)
        return;

    rx_config_gyro_mode_pos_t *modes = &m_config.gyroModeSwitch;
    rx_config_gyro_mode_pos_t newModes;
    newModes.raw = modes->raw;

    switch (pos)
    {
    case 0:
        newModes.val.pos1 = mode;
        break;
    case 1:
        newModes.val.pos2 = mode;
        break;
    case 2:
        newModes.val.pos3 = mode;
        break;
    case 3:
        newModes.val.pos4 = mode;
        break;
    case 4:
        newModes.val.pos5 = mode;
        break;
    }
    if (modes->raw == newModes.raw)
        return;

    modes->raw = newModes.raw;
    m_modified = EVENT_CONFIG_GYRO_CHANGED;
}

void GyroConfig::SetGyroPIDRate(gyro_pidgroup_t group, gyro_axis_t axis, gyro_rate_variable_t var, uint8_t new_value)
{
    rx_config_gyro_PID_t *config = &m_config.gyroPIDs[group][axis];

    uint8_t old_value = 0;
    switch (var)
    {
    case GYRO_RATE_VARIABLE_P:
        old_value = config->val.p;
        break;
    case GYRO_RATE_VARIABLE_I:
        old_value = config->val.i;
        break;
    case GYRO_RATE_VARIABLE_D:
        old_value = config->val.d;
        break;
    }

    if (new_value == old_value)
        return;

    switch (var)
    {
    case GYRO_RATE_VARIABLE_P:
        config->val.p = new_value;
        break;
    case GYRO_RATE_VARIABLE_I:
        config->val.i = new_value;
        break;
    case GYRO_RATE_VARIABLE_D:
        config->val.d = new_value;
        break;
    }

    m_modified = EVENT_CONFIG_GYRO_CHANGED;
}

void GyroConfig::SetGyroOrientation(uint8_t newOrientationH, uint8_t newOrientationV)
{
    if (m_config.global.val.orientationH != newOrientationH ||
        m_config.global.val.orientationV != newOrientationV)
    {
        m_config.global.val.orientationH = newOrientationH;
        m_config.global.val.orientationV = newOrientationV;
        m_modified = EVENT_CONFIG_GYRO_CHANGED;
    }
}

void GyroConfig::SetGyroEnabled(bool value)
{
    if (m_config.global.val.gyroEnabled != value)
    {
        m_config.global.val.gyroEnabled = value;
        m_modified = EVENT_CONFIG_GYRO_CHANGED;
    }
}

void GyroConfig::SetGyroConfigVersion(uint8_t value)
{
    if (m_config.configVersion != value)
    {
        m_config.configVersion = value;
        m_modified = EVENT_CONFIG_GYRO_CHANGED;
    }
}

void GyroConfig::SetGyroGainFactor(gyro_gain_factor_t value)
{
    if (m_config.global.val.gainFactor != value)
    {
        m_config.global.val.gainFactor = value;
        m_modified = EVENT_CONFIG_GYRO_CHANGED;
    }
}

void GyroConfig::SetAccelCalibration(uint16_t x, uint16_t y, uint16_t z)
{
    rx_config_gyro_calibration_t *accel = &m_config.accelCalibration;
    if (accel->val.x == x && accel->val.y == y && accel->val.z == z) return; // no-change
    accel->val.x = x;
    accel->val.y = y;
    accel->val.z = z;
    m_modified = EVENT_CONFIG_GYRO_CHANGED;
}

void
GyroConfig::SetGyroCalibration(uint16_t x, uint16_t y, uint16_t z)
{
    rx_config_gyro_calibration_t *gyro = &m_config.gyroCalibration;
    if (gyro->val.x == x && gyro->val.y == y && gyro->val.z == z) return; // no-change
    gyro->val.x = x;
    gyro->val.y = y;
    gyro->val.z = z;
    m_modified = EVENT_CONFIG_GYRO_CHANGED;
}

void
GyroConfig::SetPwmChannelLimits(uint8_t ch, uint16_t min, uint16_t max, uint16_t mid)
{
    if (ch > PWM_MAX_CHANNELS)
        return;

    rx_config_pwm_limits_t *pwm = &m_config.pwmLimits[ch];
    rx_config_pwm_limits_t new_limits;
    new_limits.val.min = min;
    new_limits.val.max = max;
    new_limits.val.mid = mid;

    if (pwm->raw == new_limits.raw)
        return;

    //DBGLN("*** Stored new PWM Limits for channel %d: Min: %d Max: %d Center: %d",
    //      ch, (uint16_t) pwm->val.min, (uint16_t) pwm->val.max, (uint16_t) pwm->val.mid);

    pwm->raw = new_limits.raw;
    m_modified = EVENT_CONFIG_GYRO_CHANGED;
    Commit();
}

void
GyroConfig::SetPwmChannelLimitsRaw(uint8_t ch, uint64_t raw)
{
    if (ch > PWM_MAX_CHANNELS)
        return;

    rx_config_pwm_limits_t *pwm = &m_config.pwmLimits[ch];
    if (pwm->raw == raw)
        return;

    pwm->raw = raw;
    //DBGLN("*** Stored new PWM Limits for channel %d: Min: %d Max: %d Center: %d",
    //      ch, (uint16_t) pwm->val.min, (uint16_t) pwm->val.max, (uint16_t) pwm->val.mid);
    m_modified = EVENT_CONFIG_GYRO_CHANGED;
}

#endif

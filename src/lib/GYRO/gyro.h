#pragma once
#include "targets.h"

#if defined(PLATFORM_ESP32)
#include "ahrs/ahrs.h"
#include "gyro_types.h"
#include "modes/mode.h"
#include "pid.h"

#define GYRO_CODE_VERSION 1.19

#define GYRO_US_MIN 885 // was 988
#define GYRO_US_MID 1500
#define GYRO_US_MAX 2135 // was 2012

#define radToDeg(angleInRadians) ((angleInRadians) * (float)RAD_TO_DEG)
#define degToRad(angleInDegrees) ((angleInDegrees) * (float)DEG_TO_RAD)

/**
 * Add some servo jitter feedback to the pilot after the gyro has initialized.
 */
#define GYRO_BOOT_JITTER
#ifdef GYRO_BOOT_JITTER
#define GYRO_BOOT_JITTER_US 45
#define GYRO_BOOT_JITTER_MS 175
#define GYRO_BOOT_JITTER_TIMES 4
#endif

class Gyro
{
public:
    void init(AHRS *_ahrs);
    void start();
    void reloadConfig();
    gyro_status_t getStatus();
    uint8_t getStatusBits();
    const char *getNextAction();
    gyro_mode_t getMode();
    int8_t getModePosition() const { return mode_position; }
    void mixerInput();
    void mixerOutput(uint8_t ch, uint16_t *us);
    uint8_t event();
    void calibrate();
    void reload();
    void pause();
    void StickCenterCalibration();
    void StickLimitCalibration(bool done);
    bool isStickCalibrationNeeded();
    bool isStickCenterCalibrationComplete() const { return learn_state != GYRO_LEARN_SUBTRIMS; }
    const rx_config_pwm_limits_t *getStickCalibrationLimits(uint8_t channel) const;

    float master_gain = 1.0;
    gyro_mode_t gyro_mode = GYRO_MODE_OFF;
    AHRS *ahrs = nullptr;
    // protected:

    // orientation/motion vars
    bool initialized = false;
    gyro_learn_state_t learn_state = GYRO_LEARN_OFF;
    char lastErrorText[30] {};

private:
    Mode_Base *mode_controller = nullptr;

    int8_t mode_ch = -1;
    int8_t mode_position = -1;
    int8_t gain_ch = -1;
    int8_t roll_ch = -1;
    int8_t pitch_ch = -1;
    int8_t yaw_ch = -1;
    int8_t elevon1_ch = -1;
    int8_t elevon2_ch = -1;
    int8_t vtail1_ch = -1;
    int8_t vtail2_ch = -1;

    void detect_gain(uint16_t us);
    void detect_mode(uint16_t us);
    void switch_mode(gyro_mode_t mode);
    void learn_sticks(uint8_t ch, uint16_t us);

    unsigned long pid_delay = 0;
};

extern Gyro gyro;

// configure PID controllers from LUA gains for each axis with the specified limit
// (typically 1.0). Set a limit to 0.0 to disable PID control on an axis.
void configure_pids(float roll_limit, float pitch_limit, float yaw_limit, const rx_config_gyro_fmode_t *fm);

// Helper method to configure a PID controller instance use the rx config values
void configure_pid_gains(PID *pid, const rx_config_gyro_PID_t *pid_params, int8_t gain, float max, float min);

void gyroSetConfigDefaults();
void gyroUpgrade(uint8_t version);
bool gyroIsVisible(gyro_mode_t fm, gyro_ui_vibility_t category);

extern int8_t gyro_trim_encode(int8_t n);
extern int8_t gyro_trim_decode(int8_t n);
#endif
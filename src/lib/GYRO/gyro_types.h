#pragma once
#include <stdint.h>

typedef union {
    struct {
        uint64_t max:12,
                 min:12,
                 mid:12,
                 unused: 28;
    } val;
    uint64_t raw;
} rx_config_pwm_limits_t;

typedef enum {
    GYRO_STATUS_OFF,
    GYRO_STATUS_NOT_DETECTED,
    GYRO_STATUS_NEED_RX_ORIENTATION,
    GYRO_STATUS_NEED_STICK_CAL,
    GYRO_STATUS_OK
} gyro_status_t;

/*
typedef enum
{
    GYRO_EVENT_NONE,
    GYRO_EVENT_CALIBRATE,
    GYRO_EVENT_HORIZONTAL_CALIBRATE,
    GYRO_EVENT_VERTICAL_CALIBRATE,
    GYRO_EVENT_SUBTRIMS
} gyro_event_t;
*/

typedef enum { // values are important
    GYRO_GAIN_FACTOR_0_5X,
    GYRO_GAIN_FACTOR_1X,
    GYRO_GAIN_FACTOR_1_5X, 
    GYRO_GAIN_FACTOR_2X,
} gyro_gain_factor_t;

typedef enum
{
    GYRO_MODE_OFF = 0,
    GYRO_MODE_RATE,
    GYRO_MODE_ENVELOPE,
    GYRO_MODE_LEVEL,
    GYRO_MODE_LAUNCH,
    GYRO_MODE_HOVER,
    GYRO_MODE_FUTURE1,
    GYRO_MODE_FUTURE2,
    GYRO_MODE_FUTURE3,
    GYRO_MODE_FUTURE4,
    GYRO_MODE_END,
    GYRO_MODE_LAST_ACTIVE = GYRO_MODE_HOVER,
    GYRO_MODE_MAX = GYRO_MODE_END
} gyro_mode_t;

/*
typedef enum {
    FN_IN_NONE,
    FN_IN_ROLL,
    FN_IN_PITCH,
    FN_IN_YAW,
    FN_IN_GYRO_MODE,
    FN_IN_GYRO_GAIN
} gyro_input_channel_function_t;
*/

typedef enum {
    GYRO_PID_GROUP_RATE,
    GYRO_PID_GROUP_ANGLE,
    GYRO_PID_GROUP_MADWICK,
    GYRO_PID_GROUP_END,
    GYRO_PID_GROUP_LAST_ACTIVE = GYRO_PID_GROUP_MADWICK,
    GYRO_PID_GROUP_MAX = GYRO_PID_GROUP_END
} gyro_pidgroup_t;


#define GYRO_N_AXES 3

typedef enum {
    GYRO_AXIS_ROLL,
    GYRO_AXIS_PITCH,
    GYRO_AXIS_YAW
} gyro_axis_t;

typedef enum {
    GYRO_RATE_VARIABLE_P,
    GYRO_RATE_VARIABLE_I,
    GYRO_RATE_VARIABLE_D
} gyro_rate_variable_t;


typedef enum {
    FN_NONE,
    FN_AILERON,
    FN_ELEVATOR,
    FN_RUDDER,
    FN_ELEVON,
    FN_ELEVON_R,
    FN_VTAIL,
    FN_VTAIL_R,
    FN_GYRO_MODE,
    FN_GYRO_GAIN
} gyro_output_channel_function_t;


typedef enum { 
    STICK_PRIORITY_100  =0,
    STICK_PRIORITY_75   =1, 
    STICK_PRIORITY_50   =2, 
    STICK_PRIORITY_25   =3 
} gyro_stick_priority_t;

typedef enum { 
    GYRO_LEARN_OFF, 
    GYRO_LEARN_SUBTRIMS, 
    GYRO_LEARN_LIMIT_START,
    GYRO_LEARN_LIMIT_DONE 
} gyro_learn_state_t;

typedef enum { 
    GYRO_UI_USE_RATE,
    GYRO_UI_STICK_PRIORITY,
    GYRO_UI_TRIMS, 
    GYRO_UI_GAINS, 
    GYRO_UI_MAX_ANGLE
} gyro_ui_vibility_t;



typedef struct __attribute__((packed)) {
    uint8_t p;
    uint8_t i;
    uint8_t d;
    uint8_t gain; // Deprecated
} rx_config_gyro_PID_t;

/*
typedef struct {
    uint16_t min;
    uint16_t mid;
    uint16_t max;
} rx_config_gyro_timings_t;
*/

typedef union __attribute__((packed)) {
    struct {
        uint16_t output_mode:5,  // Max 32 Modes
                 master:1,       // master channel?
                 inverted:1,     // invert gyro output?
                 unused:9;
    } val;
    uint32_t raw;
} rx_config_gyro_channel_t;

typedef union __attribute__((packed)) {
    struct {
        uint64_t gainRoll:8,
                 gainPitch:8,
                 gainYaw:8,

                 maxAnglePitch:7, // Max 90
                 maxAngleRoll:7,

                 trimPitch:6, // Max +- 31  (Any value > 31 is negative. see the helper functions for this)
                 trimRoll:6,

                 useRate:1,
                 stickPri:2,  // Max 4

                 unused:11;
    } val;
    uint64_t raw;
} rx_config_gyro_fmode_t;

typedef union __attribute__((packed)) {
    struct {
        uint32_t pos1: 4,
                 pos2: 4,
                 pos3: 4,
                 pos4: 4,
                 pos5: 4,
                 unused: 12;
    } val;
    uint32_t raw;
} rx_config_gyro_mode_pos_t;

typedef struct __attribute__((packed)) {
    int16_t x;
    int16_t y;
    int16_t z;
} rx_config_gyro_calibration_t;

constexpr uint8_t GYRO_MAX_CHANNELS = 16;
constexpr uint8_t GYRO_CONFIG_VERSION = 3;

typedef struct __attribute__((packed)) {
    uint8_t  configVersion;
    uint16_t orientationH:3,
             orientationV:3,
             gyroEnabled:1,
             gainFactor:3, // 0.5, 1, 1.5, 2
             unused_a:6;  // More global settings
 
    rx_config_gyro_calibration_t accelCalibration;
    rx_config_gyro_calibration_t gyroCalibration;
    rx_config_pwm_limits_t pwmLimits[GYRO_MAX_CHANNELS];
    rx_config_gyro_channel_t gyroChannels[GYRO_MAX_CHANNELS];

    rx_config_gyro_mode_pos_t gyroModeSwitch; // Gyro functions for switch positions
    rx_config_gyro_PID_t   gyroPIDs[GYRO_PID_GROUP_MAX][GYRO_N_AXES]; // PID gains for each axis
    rx_config_gyro_fmode_t gyroFModes[GYRO_MODE_MAX]; //Mode Description
} rx_config_gyro_t;


class Quaternion {
    public:
        float w;
        float x;
        float y;
        float z;
        
        Quaternion() {
            reset();
        }
        
        Quaternion(float nw, float nx, float ny, float nz) {
            w = nw;
            x = nx;
            y = ny;
            z = nz;
        }

        void reset() {
            w = 1.0f;
            x = 0.0f;
            y = 0.0f;
            z = 0.0f;
        }
};

class VectorInt16 {
    public:
        int16_t x;
        int16_t y;
        int16_t z;

        VectorInt16() {
            x = 0;
            y = 0;
            z = 0;
        }
        
        VectorInt16(int16_t nx, int16_t ny, int16_t nz) {
            x = nx;
            y = ny;
            z = nz;
        }
};

class VectorFloat {
    public:
        float x;
        float y;
        float z;

        VectorFloat() {
            x = 0;
            y = 0;
            z = 0;
        }
        
        VectorFloat(float nx, float ny, float nz) {
            x = nx;
            y = ny;
            z = nz;
        }
};
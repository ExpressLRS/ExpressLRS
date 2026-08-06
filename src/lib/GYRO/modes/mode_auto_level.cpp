#include "gyro.h"

#if defined(GYRO_SUPPORT)
#include "config.h"
#include "crsf_protocol.h"

#include "mode_auto_level.h"
#include "pid.h"
#include "gyro_types.h"
#include "logging.h"

/**
 * Airplane Level/Stable Mode
 *
 * This mode tries to keep the plane flying level (pitch/roll) when there is no
 * stick input.
 *
 */

LevelController::LevelController():
    RateController()
{
}

void LevelController::initialize(gyro_mode_t mode)
{
    RateController::initialize(mode);

    fm_angle_settings.raw =  config.GetGyroFMode(mode)->raw; // Default settings for ALL

    const rx_config_gyro_PID_t *roll_pid_params     = config.GetGyroPID(GYRO_PID_GROUP_ANGLE, GYRO_AXIS_ROLL);
    const rx_config_gyro_PID_t *pitch_pid_params    = config.GetGyroPID(GYRO_PID_GROUP_ANGLE, GYRO_AXIS_PITCH);
   
    float roll_limit = 1.0;
    float pitch_limit = 1.0;

    configure_pid_gains(&pid_angle_roll,  roll_pid_params,    fm_angle_settings.val.gainRoll,  roll_limit, -1.0 * roll_limit);
    configure_pid_gains(&pid_angle_pitch, pitch_pid_params,   fm_angle_settings.val.gainPitch,  pitch_limit, -1.0 * pitch_limit);

    ignore_input[0] = ignore_input[1] = ignore_input[2] = false;
}

void LevelController::calculate_pid(float input_rpy[], float gyro_rpy[], float ang_rpy[])
{
    RateController::calculate_pid(input_rpy, gyro_rpy, ang_rpy);

    // In Level mode, the Gyro has full control of [Pitch, Roll], not the command
    ignore_input[GYRO_AXIS_ROLL] = ignore_input[GYRO_AXIS_PITCH] = true;

    // Get Pitch/Roll angles adjusted to the Level trims  (global + current senttings)
    float pitch_angle = - ang_rpy[GYRO_AXIS_PITCH] + degToRad(gyro_trim_decode(fm_settings.val.trimPitch));
    float roll_angle  =   ang_rpy[GYRO_AXIS_ROLL]  + degToRad(gyro_trim_decode(fm_settings.val.trimRoll));

    if (isInverted(ang_rpy)) {
        // The pitch seems to be reported the same even when it is inverted in the roll axis
        pitch_angle *= -1; // reverse pitch
    }

    // Angle Demand
    // The stick tell the percentage of the max angle where we want the plane to be
    float setpoint_pitch =  input_rpy[GYRO_AXIS_PITCH] * degToRad(fm_angle_settings.val.maxAnglePitch);
    float setpoint_roll  =  input_rpy[GYRO_AXIS_ROLL]  * degToRad(fm_angle_settings.val.maxAngleRoll);

    pid_angle_pitch.calculate(setpoint_pitch,pitch_angle);
    pid_angle_roll.calculate(setpoint_roll,roll_angle);

    if (isInverted(ang_rpy)) {
        pid_angle_pitch.reset(); // don't apply elevator corrections if inverted
        ignore_input[GYRO_AXIS_PITCH] = false; // But let the stick controll it.
    }

    if (isHighPitch(ang_rpy)) {
        pid_angle_roll.reset(); // Roll does not work that well in high pitch angles (80 deg)
        ignore_input[GYRO_AXIS_ROLL] = false; // Allow the stick to command roll
    }

    // Add angle correction to rate corrections ajusted to angle Gains
    corr[GYRO_AXIS_ROLL]  += pid_angle_roll.output;
    corr[GYRO_AXIS_PITCH] += pid_angle_pitch.output;   
}

#if defined(DEBUG_LOG)
void LevelController::printState() {
    RateController::printState();
    DBGLN("IsInverted = %d,  IsHighPitch=%d", isInverted(gyro.ahrs->angle_rpy), isHighPitch(gyro.ahrs->angle_rpy));
    DBGLN("IgnoreCmd:  Roll:%d Pitch:%d ", ignore_input[GYRO_AXIS_ROLL], ignore_input[GYRO_AXIS_PITCH]);
    DBGLN("Ang Corr:   Roll:%f Pitch:%f", pid_angle_roll.output, pid_angle_pitch.output);

}
#endif // DEBUG_LOG

#endif

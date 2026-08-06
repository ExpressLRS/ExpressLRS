#if defined(GYRO_SUPPORT)

#include "gyro.h"
#include "mode_rate.h"
#include "pid.h"
#include "logging.h"
#include "utils.h"

/**
 * Airplane Normal Mode / Rate Mode
 *
 * This is a basic "wind rejection mode" and counteracts roll and pitch changes.
 *
 * As the channel command increases the correction decreases allowing unlimited
 * angular rates.
 */

void _make_gyro_debug_string(PID *pid, char *str) {
    sprintf(str, "Setpoint: %5.2f PV: %5.2f I:%5.2f D:%5.2f Error: %5.2f Out: %5.2f",
        pid->setpoint, pid->pv, pid->Iout, pid->Dout, pid->error, pid->output);

}

 RateController::RateController()
 {
 }

void RateController::configure_pid_gains(PID *pid, const rx_config_gyro_PID_t *pid_params, int8_t gain,
                         float max, float min)
{
    DBG("Config gains: [P=%d I=%d D=%d G=%d] ", pid_params->p, pid_params->i, pid_params->d, (int8_t) gain);
    if (max == 0.0 && min == 0.0) {
        // not gyro correction on this axis
        pid->configure(0.0, 0.0, 0.0, 0.0, 0.0);
    } else {
        float p = gain * pid_params->p / 1000.0;
        float i = gain * pid_params->i / 1000.0;
        float d = gain * pid_params->d / 1000.0;
        DBG("PID: [P=%f I=%f D=%f]", p, i, d);

        pid->configure(p, i, d, max, min);
    }
    DBGLN("");
    pid->reset();
}

void  RateController::applyFModeSettings(gyro_mode_t fm) {
    fm_settings.raw =  config.GetGyroFMode(GYRO_MODE_RATE)->raw; // Default settings for ALL

    auto rate_settings = config.GetGyroFMode(fm); // Get settings for Specific flight Mode

    if (fm == GYRO_MODE_LEVEL || fm == GYRO_MODE_ENVELOPE) { // Override
        fm_settings.val.maxAnglePitch = rate_settings->val.maxAnglePitch;
        fm_settings.val.maxAngleRoll = rate_settings->val.maxAngleRoll;
    } else {
        fm_settings.val.maxAnglePitch=0;
        fm_settings.val.maxAngleRoll=0;
    }

    if (fm == GYRO_MODE_LEVEL || fm == GYRO_MODE_LAUNCH) { 
        fm_settings.val.trimPitch = (int8_t) rate_settings->val.trimPitch;
        fm_settings.val.trimRoll  = (int8_t) rate_settings->val.trimRoll;
    } else {
        fm_settings.val.trimPitch=0;
        fm_settings.val.trimRoll=0;
    }

     if (fm != GYRO_MODE_RATE) { 
        fm_settings.val.useRate = (int8_t) rate_settings->val.useRate;
     } else {
        fm_settings.val.useRate = true;
     }


    if (fm_settings.val.maxAnglePitch+fm_settings.val.maxAngleRoll > 0) {
        DBGLN("Angle Limits: Pitch=%d Roll=%d",(int8_t)fm_settings.val.maxAnglePitch, (int8_t)fm_settings.val.maxAngleRoll);
    }

    if (fm_settings.val.trimPitch != 0 || fm_settings.val.trimRoll != 0) {
       DBGLN("Trims: Pitch=%d roll=%d",gyro_trim_decode(fm_settings.val.trimPitch), gyro_trim_decode(fm_settings.val.trimRoll));
    }
}


bool RateController::isInverted(float angle_rpy[]) {
    float angleDeg = radToDeg(fabs(angle_rpy[GYRO_AXIS_ROLL]));
    return angleDeg > 110;
}

bool RateController::isHighPitch(float angle_rpy[]) {
    float angleDeg = radToDeg(fabs(angle_rpy[GYRO_AXIS_PITCH]));
    return angleDeg > 80;
}

void RateController::initialize(gyro_mode_t mode)
{
    RateController::mode = mode;

    float roll_limit = 1.0;
    float pitch_limit = 1.0;
    float yaw_limit = 1.0;

    applyFModeSettings(mode);

    const rx_config_gyro_PID_t *roll_pid_params     = config.GetGyroPID(GYRO_PID_GROUP_RATE, GYRO_AXIS_ROLL);
    const rx_config_gyro_PID_t *pitch_pid_params    = config.GetGyroPID(GYRO_PID_GROUP_RATE, GYRO_AXIS_PITCH);
    const rx_config_gyro_PID_t *yaw_pid_params      = config.GetGyroPID(GYRO_PID_GROUP_RATE, GYRO_AXIS_YAW);

    configure_pid_gains(&pid_roll,  roll_pid_params,    fm_settings.val.gainRoll,   roll_limit, -1.0 * roll_limit);
    configure_pid_gains(&pid_pitch, pitch_pid_params,   fm_settings.val.gainPitch,  pitch_limit, -1.0 * pitch_limit);
    configure_pid_gains(&pid_yaw,   yaw_pid_params,     fm_settings.val.gainYaw,    yaw_limit, -1.0 * yaw_limit);

    ignore_input[0] = ignore_input[1] = ignore_input[2] = false;
}

void RateController::calculate_stick_pri(float input_rpt[]) {
    // Modulate the correction depending on how much axis stick command
    //float stick_gain_div = (1 << stick_priority); // 1,2,4
    //float percent = (1/stick_gain_div)-fabs(roll_in);
    //if (percent < 0) percent = 0.0;

    // Modulate the correction depending on how much axis stick command
    for (int8_t axis = 0; axis < 3; axis++) {
        stick_pri[axis]  = 1 - fabs(input_rpy[axis]);
    }
}

void RateController::calculate_pid(float input_rpy[], float gyro_rpy[], float ang_rpy[])
{
    ignore_input[0] = ignore_input[1] = ignore_input[2] = false;

    // Copy parameters to internal class variables, and initialize vars
    for (int8_t axis = 0; axis < 3; axis++) {
        RateController::input_rpy[axis] = input_rpy[axis];
        corr[axis] = 0;
        stick_pri[axis] = 0;
    }

    // If this mode don't use Rate? don't compute corrections
    if (!fm_settings.val.useRate) {
        pid_roll.reset();
        pid_pitch.reset();
        pid_yaw.reset();
        return;
    }

    calculate_stick_pri(input_rpy);
    
    // Adjustment Rate, otherwise it becomes too sensitive
    const float ADJUSTMENT_FACTOR = 0.25  * gyro.gain_factor;
    
    // Desired angular rate is zero
    pid_roll.calculate(0,   gyro_rpy[GYRO_AXIS_ROLL]  * ADJUSTMENT_FACTOR);
    pid_pitch.calculate(0,  gyro_rpy[GYRO_AXIS_PITCH] * ADJUSTMENT_FACTOR);
    pid_yaw.calculate(0,   -gyro_rpy[GYRO_AXIS_YAW]   * ADJUSTMENT_FACTOR);


    float total_gain =  gyro.master_gain;
    corr[GYRO_AXIS_ROLL]  = pid_roll.output  * stick_pri[GYRO_AXIS_ROLL]  * total_gain ;
    corr[GYRO_AXIS_PITCH] = pid_pitch.output * stick_pri[GYRO_AXIS_PITCH] * total_gain;
    corr[GYRO_AXIS_YAW]   = pid_yaw.output   * stick_pri[GYRO_AXIS_YAW]   * total_gain;
}

#if defined(DEBUG_LOG)
void RateController::printState() {
        char piddebug[128];
        DBGLN("TOTAL MASTER GAIN %f.  IMU: Read Err=%d Int_Err=%d", gyro.master_gain * gyro.gain_factor, gyro.ahrs->read_errors, gyro.ahrs->int_errors);
        sprintf(piddebug,"Roll:%5.2f Pitch:%5.2f Yaw:%5.2f", radToDeg(gyro.ahrs->angle_rpy[0]), radToDeg(gyro.ahrs->angle_rpy[1]), radToDeg(gyro.ahrs->angle_rpy[2])); 
        DBGLN("Angles:  %s",piddebug);
        sprintf(piddebug,"Roll:%5.2f Pitch:%5.2f Yaw:%5.2f", input_rpy[GYRO_AXIS_ROLL], input_rpy[GYRO_AXIS_PITCH], input_rpy[GYRO_AXIS_YAW]);    
        DBGLN("Cmds:    %s",piddebug);

        sprintf(piddebug,"Roll:%5.2f Pitch:%5.2f Yaw:%5.2f", stick_pri[GYRO_AXIS_ROLL], stick_pri[GYRO_AXIS_PITCH], stick_pri[GYRO_AXIS_YAW]);
        DBGLN("StickPri:%s",piddebug);
        sprintf(piddebug,"Roll:%5.2f Pitch:%5.2f Yaw:%5.2f",corr[GYRO_AXIS_ROLL], corr[GYRO_AXIS_PITCH], corr[GYRO_AXIS_YAW]);
        DBGLN("Corr    :%s",piddebug);

        _make_gyro_debug_string(&pid_pitch, piddebug);
        DBGLN("PID Pitch %s", piddebug);
        _make_gyro_debug_string(&pid_roll, piddebug);
        DBGLN("PID Roll  %s", piddebug);
        _make_gyro_debug_string(&pid_yaw, piddebug);
        DBGLN("PID Yaw   %s", piddebug);
}
#endif 

uint16_t RateController::applyCorrection(uint8_t ch, gyro_output_channel_function_t channel_function, float command, bool inverted) {
    float correction = 0.0;
    if (channel_function==FN_AILERON) {
        correction = (inverted)?-corr[GYRO_AXIS_ROLL]:corr[GYRO_AXIS_ROLL];
        if (ignore_input[GYRO_AXIS_ROLL]) {
            command = 0;
        }
    } else if (channel_function==FN_ELEVATOR) {
        correction = (inverted)?-corr[GYRO_AXIS_PITCH]:corr[GYRO_AXIS_PITCH];
        if (ignore_input[GYRO_AXIS_PITCH]) {
            command = 0;
        }
    } 
    else if (channel_function==FN_ELEVON || channel_function==FN_ELEVON_R) {
        correction = (inverted)?-corr[GYRO_AXIS_PITCH]:corr[GYRO_AXIS_PITCH]; // Invert elevator if needed
        auto cor2 = (channel_function==FN_ELEVON)?corr[GYRO_AXIS_ROLL]:-corr[GYRO_AXIS_ROLL]; // Invert Aileron depending of Left/Right 
        correction = (correction + cor2)/2;
        if (ignore_input[GYRO_AXIS_PITCH]) {
            command = 0;
        }
    }
    else if (channel_function==FN_VTAIL || channel_function==FN_VTAIL_R) {
        correction = (inverted)?-corr[GYRO_AXIS_PITCH]:corr[GYRO_AXIS_PITCH]; // Invert elevator if needed
        auto cor2 = (channel_function==FN_VTAIL)?corr[GYRO_AXIS_YAW]:-corr[GYRO_AXIS_YAW]; // Invert Rud depending of Left/Right 
        correction = (correction + cor2)/2;
        if (ignore_input[GYRO_AXIS_PITCH]) {
            command = 0;
        }
    }
    else if (channel_function==FN_RUDDER) {
        correction = (inverted)?-corr[GYRO_AXIS_YAW]:corr[GYRO_AXIS_YAW]; // Invert rudder if needed
    }

    return float_to_us(ch, command + correction);
}

#endif

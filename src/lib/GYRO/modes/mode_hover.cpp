#include "mode_hover.h"
#include "config.h"
#include "logging.h"

#if defined(GYRO_SUPPORT)
/**
 * Airplane Hover Mode
 *
 * For hover mode we care about two angles
 *
 * 1. The "pitch" angle of the horizon.
 *    This is the amount of error.
 *
 * 2. Rotation around the "roll" axis.
 *    This is the modulation between elevator and rudder to correct the error.
 *
 * With these angles we can command elevator and rudder to correct towards a nose
 * directly up attitude.
 */

void HoverController::initialize(gyro_mode_t mode) {
    RateController::initialize(mode);

    fm_angle_settings.raw =  config.GetGyroFMode(mode)->raw; // Default settings for ALL

    hoverStrengthPitch = (float) fm_angle_settings.val.gainPitch / 100;
    hoverStrengthYaw   = (float) fm_angle_settings.val.gainYaw / 100;
}

void HoverController::calculate_pid(float input_rpy[], float gyro_rpy[], float ang_rpy[])
{
    RateController::calculate_pid(input_rpy, gyro_rpy, ang_rpy);
    
    float pitchRad = ang_rpy[GYRO_AXIS_PITCH];
    float rollRad =  ang_rpy[GYRO_AXIS_ROLL];

    float error = pitchRad - M_PI_2; // Pi/2 = 90degrees

    //NOTE: ORIGINAL A.Wigen CODE DID NOT HAVE THIS NORMALIZATION
    // Normalize Error to +/- 1.0;  
    error = error / M_PI;

    errorPitch =  error * hoverStrengthPitch;
    errorYaw   = -error * hoverStrengthYaw;
    
    corr[GYRO_AXIS_PITCH] += (errorPitch * cos(rollRad));
    corr[GYRO_AXIS_YAW]   += (errorYaw * sin(rollRad));
}

#if defined(DEBUG_LOG)
void HoverController::printState() {
    RateController::printState();
    DBGLN("error:  Pitch:%f", errorPitch);
    DBGLN("hoverStrength:  Pitch:%f Yaw:%f", hoverStrengthPitch, hoverStrengthYaw);
    DBGLN("angCorr:  Pitch:%f  Yaw:%f", corr[GYRO_AXIS_PITCH], corr[GYRO_AXIS_YAW]);

}
#endif // DEBUG_LOG

#endif

#include "gyro.h"

#if defined(GYRO_SUPPORT)
#include "config.h"
#include "crsf_protocol.h"
#include "mode_envelope.h"
#include "pid.h"
#include "gyro_types.h"
#include "logging.h"

#define   CENTER_DEADBAND   0.1  // 10% of stick movement is deadband
/**
 * Airplane SAFE Mode
 *
 * This mode will prevent the plane from exceding the MAX angles.
 *
 */

 typedef enum {
    ANGLE_STATE_OFF=0,
    ANGLE_STATE_AT_MAX,
    ANGLE_STATE_REVERSING
 } AngleLockState;


 /**
 * Manage a state machine when we reach an angle.
 * We should be able to get to the desire max angle, but after that, disable the stick 
 * action (more angle), until the stick direction changes.. Then the stick becomes effective again.
 * Similar Behaviour to Spektrum AS3X/SAFE 
 */
 class AngleLock {
    public:
        AngleLockState state = ANGLE_STATE_OFF;
        int8_t cmd_dir = 0;
        bool   ignore_cmd = false;
        
        void reset() {
            state = ANGLE_STATE_OFF;
            cmd_dir = 0;
            ignore_cmd = false;
        }

        int8_t ignoreCommand() { return ignore_cmd; }
        
        /* manage gyro at different states */
        void compute_pid (PID *pid, float angle, float max_angle, float cmd_in) {
            ignore_cmd = false;

            if (state==ANGLE_STATE_OFF) { // Still below angle limits
                if (abs(angle) > max_angle) { // Past angle limit??
                    state = ANGLE_STATE_AT_MAX; // At Angle Limit
                    DBGLN("Envelope(Ang:%f): MAX Angle Locked",radToDeg(angle));
                } else {
                    // Normal operation, below angle limit
                    pid->reset();
                }
            } else
            if (state==ANGLE_STATE_AT_MAX) { // Above angle limit
                ignore_cmd = false;
                int8_t ang_dir = (angle <0?-1:+1);
                int8_t stick_dir = (cmd_in<0?-1:+1); // Sign/direction of stick

                if (abs(angle) < max_angle && stick_dir!=ang_dir) { // Angle Back to normal, and stick trying to decrease bank??
                    DBGLN("Envelope(): Back to normal Angle");
                    ignore_cmd = false;
                    pid->reset();
                    state = ANGLE_STATE_OFF;  // back to normal
                } else {
                    // Still past MAX angle
                    if (stick_dir!=ang_dir)   // Stick trying to degrease bank angle (Oposite direction)
                    { 
                        // Let the stick have control
                        pid->reset(); // 0 correction
                        ignore_cmd = false;  // stick is effective again, decreasing angle  
                    } else {
                        // Gyro Envelope keeps control
                        float setpoint = angle > 0 ? max_angle : - max_angle;
                        pid->calculate(setpoint,angle);
                        ignore_cmd = true;
                    }
                }
            } 
        };
 };


static AngleLock AngleLockPitch;
static AngleLock AngleLockRoll;

void AngEnvelopeController::initialize(gyro_mode_t mode)
{
    RateController::initialize(mode);

    fm_angle_settings.raw =  config.GetGyroFMode(mode)->raw; // Default settings for ALL

    const rx_config_gyro_PID_t *roll_pid_params     = config.GetGyroPID(GYRO_PID_GROUP_ANGLE, GYRO_AXIS_ROLL);
    const rx_config_gyro_PID_t *pitch_pid_params    = config.GetGyroPID(GYRO_PID_GROUP_ANGLE, GYRO_AXIS_PITCH);
   
    float roll_limit = 1.0;
    float pitch_limit = 1.0;

    configure_pid_gains(&pid_angle_roll,  roll_pid_params,    fm_angle_settings.val.gainRoll,  roll_limit,  -1.0 * roll_limit);
    configure_pid_gains(&pid_angle_pitch, pitch_pid_params,   fm_angle_settings.val.gainPitch, pitch_limit, -1.0 * pitch_limit);

    // Reset angle lock state
    AngleLockPitch.reset();
    AngleLockRoll.reset();
}


void AngEnvelopeController::calculate_pid(float input_rpy[], float gyro_rpy[], float ang_rpy[])
{
    RateController::calculate_pid(input_rpy, gyro_rpy, ang_rpy);

    // Adjust angle with Level Trims
    float pitch_angle = - ang_rpy[GYRO_AXIS_PITCH] + degToRad(gyro_trim_decode(fm_settings.val.trimPitch));
    float roll_angle  = ang_rpy[GYRO_AXIS_ROLL] + degToRad(gyro_trim_decode(fm_settings.val.trimRoll));

    if (isInverted(ang_rpy)) {
        // The pitch seems to be reported the same even when it is inverted in the roll axis
        pitch_angle *= -1; // reverse pitch
    }

    AngleLockPitch.compute_pid(&pid_angle_pitch, pitch_angle, degToRad(fm_angle_settings.val.maxAnglePitch), input_rpy[GYRO_AXIS_PITCH]);
    AngleLockRoll.compute_pid(&pid_angle_roll, roll_angle, degToRad(fm_angle_settings.val.maxAngleRoll), input_rpy[GYRO_AXIS_ROLL]);

    ignore_input[GYRO_AXIS_PITCH] = AngleLockPitch.ignoreCommand();
    ignore_input[GYRO_AXIS_ROLL]  = AngleLockRoll.ignoreCommand();

    /*
    if (isInverted(ang_rpy)) {
        pid_angle_pitch.reset(); // don't apply elevator corrections if inverted
        ignore_input[GYRO_AXIS_PITCH] = false; // But let the stick controll it.
    }

    if (isHighPitch(ang_rpy)) {
        pid_angle_roll.reset(); // Roll does not work that well in high pitch angles (80 deg)
        ignore_input[GYRO_AXIS_ROLL] = false; // Allow the stick to command roll
    }
    */

    // Add angle correction to rate corrections
    corr[GYRO_AXIS_ROLL]  += pid_angle_roll.output;
    corr[GYRO_AXIS_PITCH] += pid_angle_pitch.output;
}

#if defined(DEBUG_LOG)
void AngEnvelopeController::printState() {
    RateController::printState();

    DBGLN("IgnoreCmd:  Roll:%d Pitch:%d ", ignore_input[GYRO_AXIS_ROLL], ignore_input[GYRO_AXIS_PITCH]);
    DBGLN("Ang Corr:   Roll:%f Pitch:%f", pid_angle_roll.output, pid_angle_pitch.output);
}
#endif // DEBUG_LOG

#endif

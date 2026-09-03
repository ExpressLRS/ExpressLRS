#include "targets.h"

#if defined(PLATFORM_ESP32)
#include "gyro.h"

#include "config.h"
#include "gyro_types.h"
#include "utils.h"

#include "ahrs/ahrs.h"
#include "crsf_protocol.h"
#include "devGyro.h"
#include "logging.h"
#include "modes/mode_auto_level.h"
#include "modes/mode_envelope.h"
#include "modes/mode_hover.h"
#include "modes/mode_rate.h"

// Comment to Remove Debug of State
#if defined(DEBUG_LOG)
#define GYRO_PID_DEBUG_TIME 5000 // Time im Ms

#ifdef GYRO_PID_DEBUG_TIME
unsigned long gyro_debug_time = 0;
#endif

#endif // DEBUG_LOG

#define GYRO_SUBTRIM_INIT_SAMPLES 10
static uint8_t stick_subtrim_cycles = 0;
static rx_config_pwm_limits_t temp_limits[PWM_MAX_CHANNELS] = {};

// Must match gyro.h gyro_mode_t
const char *STR_gyroMode[] = {"Off", "Rate", "Envelope", "Auto-Level", "Launch", "Hover"};

static Mode_Base *mode_controllers[GYRO_MODE_LAST_ACTIVE + 1] = {};
static bool first_start = true;

#ifdef GYRO_BOOT_JITTER
static uint8_t boot_jitter_times = 0;
static uint32_t boot_jitter_time = 0;
static int8_t boot_jitter_offset = GYRO_BOOT_JITTER_US;

static bool boot_jitter(uint16_t *us)
{
    if (boot_jitter_times > GYRO_BOOT_JITTER_TIMES)
        return false;

    if ((millis() - boot_jitter_time) > GYRO_BOOT_JITTER_MS)
    {
        boot_jitter_times++;
        boot_jitter_time = millis();
        boot_jitter_offset *= -1;
    }

    *us = *us + boot_jitter_offset;
    return true;
}
#endif

/**
 * Return the first channel matching input `mode` or -1 if not found.
 */
static int8_t GetGyroFunChannelNumber(gyro_output_channel_function_t mode, gyro_output_channel_function_t mode2 = (gyro_output_channel_function_t)100, int8_t start_ch = 0)
{
    int8_t result = -1;
    for (int8_t i = start_ch; i < GYRO_MAX_CHANNELS; i++)
    {
        auto info = gyroConfig->GetGyroChannel(i);
        if (info->val.output_mode == mode ||
            info->val.output_mode == mode2)
        {
            if (result == -1)
            {
                result = i; // Minimum Ch that is that mode, check if it is the master
            }
            if (info->val.master)
            {
                // Found the Master, no need to look for more
                return i;
            }
        }
    }
    return result;
}

static uint16_t channel_us(uint8_t ch)
{
    const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
    const unsigned crsfVal = ChannelData[chConfig->val.inputChannel];
    return CRSF_to_US(crsfVal);
}

static uint16_t channel_crsf(uint8_t ch)
{
    const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
    return ChannelData[chConfig->val.inputChannel];
}

static float channel_command(uint8_t ch)
{
    const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
    const unsigned crsfVal = ChannelData[chConfig->val.inputChannel];
    if (crsfVal == CRSF_CHANNEL_VALUE_UNSET)
        return 0;
    uint16_t us = CRSF_to_US(crsfVal);
    return us_command_to_float(ch, us);
}

void Gyro::init(AHRS *_ahrs)
{
    DBGLN("Gyro Init");

    this->ahrs = _ahrs;
    initialized = false;
    mode_controller = nullptr;
    gyro_mode = GYRO_MODE_OFF;
    learn_state = GYRO_LEARN_OFF;

    mode_controllers[GYRO_MODE_OFF] = nullptr;
    mode_controllers[GYRO_MODE_RATE] = new RateController();
    mode_controllers[GYRO_MODE_LEVEL] = new LevelController();
    mode_controllers[GYRO_MODE_LAUNCH] = mode_controllers[GYRO_MODE_LEVEL];
    mode_controllers[GYRO_MODE_ENVELOPE] = new AngEnvelopeController();
    mode_controllers[GYRO_MODE_HOVER] = new HoverController();
}

void Gyro::start()
{
    DBGLN("Gyro Start");
    initialized = false;
    ahrs->start();
    reloadConfig();

#ifdef GYRO_BOOT_JITTER
    if (first_start && (connectionState == connected))
    {
        boot_jitter_times = 0;
        boot_jitter_time = 0;
        first_start = false;
    }
#endif
}

void Gyro::reloadConfig()
{
    initialized = false;
    mode_position = -1;
    if (!gyroConfig->GetGyroEnabled())
        return; // not enabled
    if (ahrs->getImuDriver() == nullptr)
        return; // No Gyro Detected

    gyro_mode = GYRO_MODE_OFF;
    learn_state = GYRO_LEARN_OFF;
    initialized = ahrs->isRunning() && !isStickCalibrationNeeded();

    mode_controller = nullptr;
    mode_ch = GetGyroFunChannelNumber(FN_GYRO_MODE);
    gain_ch = GetGyroFunChannelNumber(FN_GYRO_GAIN);
    roll_ch = GetGyroFunChannelNumber(FN_AILERON);
    pitch_ch = GetGyroFunChannelNumber(FN_ELEVATOR);
    yaw_ch = GetGyroFunChannelNumber(FN_RUDDER);

    elevon1_ch = GetGyroFunChannelNumber(FN_ELEVON, FN_ELEVON_R);
    if (elevon1_ch >= 0)
    {
        elevon2_ch = GetGyroFunChannelNumber(FN_ELEVON, FN_ELEVON_R, (int8_t)(elevon1_ch + 1));
        pitch_ch = -1; // Ignore the individual inputs, will use Elevons as Pitch/Roll
        roll_ch = -1;
    }

    vtail1_ch = GetGyroFunChannelNumber(FN_VTAIL, FN_VTAIL_R);
    if (vtail1_ch >= 0)
    {
        vtail2_ch = GetGyroFunChannelNumber(FN_VTAIL, FN_VTAIL_R, (int8_t)(vtail1_ch + 1));
        pitch_ch = -1; // Ignore the individual inputs, will use VTail as Pitch/Yaw
        yaw_ch = -1;
    }
}

gyro_status_t Gyro::getStatus()
{
    if (!gyroConfig->GetGyroEnabled())
        return GYRO_STATUS_OFF;
    if (ahrs->getImuDriver() == nullptr)
        return GYRO_STATUS_NOT_DETECTED;
    if ((getStatusBits() & GYRO_STATUS_BIT_ORIENTATION) == 0)
        return GYRO_STATUS_NEED_RX_ORIENTATION;
    if ((getStatusBits() & GYRO_STATUS_BIT_STICK_CAL) == 0)
        return GYRO_STATUS_NEED_STICK_CAL;
    return GYRO_STATUS_OK;
}

gyro_mode_t Gyro::getMode()
{
    return gyro_mode;
}

uint8_t Gyro::getStatusBits()
{
    uint8_t bits = 0;

    const rx_config_gyro_calibration_t *accelCalibration = gyroConfig->GetAccelCalibration();
    const rx_config_gyro_calibration_t *gyroCalibration = gyroConfig->GetGyroCalibration();
    if (accelCalibration->raw != 0 && gyroCalibration->raw != 0)
        bits |= GYRO_STATUS_BIT_LEVEL_CAL;

    if (gyroConfig->GetGyroOrientationH() <= 5 && gyroConfig->GetGyroOrientationV() <= 5)
        bits |= GYRO_STATUS_BIT_ORIENTATION;

    if (!isStickCalibrationNeeded())
        bits |= GYRO_STATUS_BIT_STICK_CAL;

    return bits;
}

const char *Gyro::getNextAction()
{
    if (!gyroConfig->GetGyroEnabled())
        return "Enable gyro";
    if (ahrs->getImuDriver() == nullptr)
        return "IMU not detected";

    const uint8_t bits = getStatusBits();
    if ((bits & GYRO_STATUS_BIT_ORIENTATION) == 0)
        return "Run Orientation Wizard";
    if ((bits & GYRO_STATUS_BIT_LEVEL_CAL) == 0)
        return "Run Level Cal";
    if ((bits & GYRO_STATUS_BIT_STICK_CAL) == 0)
        return "Run Stick Calibration";
    return "Ready";
}

void Gyro::calibrate()
{
    initialized = false;
    first_start = true;
    // Level Calibration
    ahrs->calibrate(true);
    initialized = ahrs->isRunning();
}

void Gyro::detect_mode(uint16_t crsf)
{
    if (crsf == CRSF_CHANNEL_VALUE_UNSET)
        return;

    mode_position = (int8_t)CRSF_to_N(crsf, gyroConfig->GetGyroModePositions());
    const gyro_mode_t selected_mode = gyroConfig->GetGyroMode(mode_position);
    if (gyro_mode != selected_mode)
        switch_mode(selected_mode);
}

/**
 * Trigger a ahrs to stop until restarted
 */
void Gyro::pause()
{
    initialized = false;
    ahrs->pause();
}

/**
 * Trigger a gyro re-initialization of the current gyro mode
 */
void Gyro::reload()
{
    pause();
    start();
}

void Gyro::switch_mode(gyro_mode_t mode)
{
    DBGLN("Gyro: Switching mode=[%s]", STR_gyroMode[mode]);

    gyro_mode = mode;
    mode_controller = mode_controllers[mode];

    if (mode_controller != nullptr)
    {
        mode_controller->initialize(mode);
    }
}

void Gyro::detect_gain(uint16_t us)
{
    master_gain = (us_command_to_float(us) + 1) / 2;
}

void Gyro::mixerInput()
{
    // We get called before the gyro configuration is initialized
    if (!initialized || learn_state != GYRO_LEARN_OFF || !ahrs->isRunning())
        return;

    const uint32_t now = micros();
    if (now - pid_delay < 1000)
        return; // ~1k PID loop
    pid_delay = now;

    if (mode_ch >= 0)
        detect_mode(channel_crsf(mode_ch));
    if (mode_controller == nullptr)
        return;

    // if (data_ready==0) return;
    // data_ready = 0;

    float input_rpy[3] = {0.0, 0.0, 0.0};

    if (roll_ch >= 0)
    {
        const auto info = gyroConfig->GetGyroChannel(roll_ch);
        input_rpy[GYRO_AXIS_ROLL] = channel_command(roll_ch) * (info->val.inverted ? -1.0f : +1.0f);
    }

    if (pitch_ch >= 0)
    {
        const auto info = gyroConfig->GetGyroChannel(pitch_ch);
        input_rpy[GYRO_AXIS_PITCH] = channel_command(pitch_ch) * (info->val.inverted ? -1.0f : +1.0f);
    }

    if (yaw_ch >= 0)
    {
        const auto info = gyroConfig->GetGyroChannel(yaw_ch);
        input_rpy[GYRO_AXIS_YAW] = channel_command(yaw_ch) * (info->val.inverted ? -1.0f : +1.0f);
    }

    // ELEVON LOGIC if no aileron/elevator
    if (elevon1_ch >= 0 && elevon2_ch >= 0)
    {
        const auto i1 = gyroConfig->GetGyroChannel(elevon1_ch);
        const auto i2 = gyroConfig->GetGyroChannel(elevon2_ch);

        const auto e1 = channel_command(elevon1_ch) * (i1->val.inverted ? -1.0f : +1.0f);
        const auto e2 = channel_command(elevon2_ch) * (i2->val.inverted ? -1.0f : +1.0f);

        // In the Radio, the Elevons are +50% ele, and +/-50% aileron
        // Pitch: The average of the two elevons, since both moves in the same direction.
        //          This gives a new "center".  So (e1 + e2) / 2.  Since the TX mix weight is 50%, then x2
        //          (e1 + e2) / 2 * 2 = (e1 + e2).
        // Roll:  Is how far e1 moved from the new "pitch" center, e1 x 2 to compensate for the 50% weight:
        //          2 * e1 - (e1+e2) = 2*e1 -e1 - e2 = (e1-e2)

        input_rpy[GYRO_AXIS_PITCH] = (e1 + e2);
        input_rpy[GYRO_AXIS_ROLL] = -(e1 - e2);

        // TODO? Do we need to invet roll??  ((i1->val.output_mode==FN_ELEVON_R)?-1:+1);
    }

    if (vtail1_ch >= 0 && vtail2_ch >= 0)
    {
        // Try VTail
        const auto i1 = gyroConfig->GetGyroChannel(vtail1_ch);
        const auto i2 = gyroConfig->GetGyroChannel(vtail2_ch);

        const auto v1 = channel_command(vtail1_ch) * (i1->val.inverted ? -1.0f : +1.0f);
        const auto v2 = channel_command(vtail2_ch) * (i2->val.inverted ? -1.0f : +1.0f);

        input_rpy[GYRO_AXIS_PITCH] = v1 + v2;
        input_rpy[GYRO_AXIS_YAW] = -(v1 - v2);

        // TODO? Do we need to invert YAW??  ((i1->val.output_mode==FN_VTAIL_R)?-1:+1);
    }

    if (gain_ch >= 0)
        detect_gain(channel_us(gain_ch));
    else
        master_gain = 1.0;

    mode_controller->calculate_pid(input_rpy, ahrs->gyro_rpy, ahrs->angle_rpy);

#if defined(DEBUG_LOG) && defined(GYRO_PID_DEBUG_TIME)
    if (gyro.gyro_mode != GYRO_MODE_OFF &&
        micros() - gyro_debug_time > GYRO_PID_DEBUG_TIME * 1000)
    {
        mode_controller->printState();
        gyro_debug_time = micros();
    }
#endif
}

/**
 * Apply gyro servo output mixing and detect gyro mode
 */
void Gyro::mixerOutput(uint8_t ch, uint16_t *us)
{
    const auto ch_info = gyroConfig->GetGyroChannel(ch);
    const auto output_mode = (gyro_output_channel_function_t)ch_info->val.output_mode;

    // Learning Sticks can happen at any time
    if (learn_state != GYRO_LEARN_OFF)
    {
        learn_sticks(ch, *us);
        return;
    }

    // We get called before the gyro configuration is initialized
    if (!initialized || !ahrs->isRunning())
        return;

    if (output_mode == FN_NONE)
        return;

#ifdef GYRO_BOOT_JITTER
    if (boot_jitter(us))
        return;
#endif

    if (mode_controller == nullptr)
        return; // Gyro OFF???

    // Normalize the µs value to a +-1.0 keeping in mind subtrim and max throws
    const float command = us_command_to_float(ch, *us);
    *us = mode_controller->applyCorrection(ch, output_mode, command, ch_info->val.inverted);

    // Limit output values to configured limits when is a channel controlled by Gyro
    if (output_mode != FN_NONE)
    {
        const rx_config_pwm_limits_t *limits = gyroConfig->GetPwmChannelLimits(ch);
        *us = constrain(*us, limits->val.min, limits->val.max);
    }
}

uint8_t Gyro::event()
{
    return DURATION_IGNORE;
}

void Gyro::learn_sticks(uint8_t ch, uint16_t us)
{
    if (learn_state == GYRO_LEARN_SUBTRIMS)
    {
        // Set midpoint (subtrim) from an average of a set of samples
        if (ch == 0 && ++stick_subtrim_cycles > GYRO_SUBTRIM_INIT_SAMPLES)
        {
            // Completed
            learn_state = GYRO_LEARN_OFF;
            return;
        }

        // Average over 10 cycles
        if (stick_subtrim_cycles < GYRO_SUBTRIM_INIT_SAMPLES)
        {
            const auto ch_limit = &temp_limits[ch];
            ch_limit->val.mid = (((ch_limit->val.mid * stick_subtrim_cycles) / stick_subtrim_cycles) + us) / 2;
        }
    }
    else if (learn_state == GYRO_LEARN_LIMIT_START)
    {
        const auto ch_limit = &temp_limits[ch];
        ch_limit->val.min = min((uint16_t)ch_limit->val.min, us);
        ch_limit->val.max = max((uint16_t)ch_limit->val.max, us);
    }
}

const rx_config_pwm_limits_t *Gyro::getStickCalibrationLimits(uint8_t channel) const
{
    return channel < PWM_MAX_CHANNELS ? &temp_limits[channel] : nullptr;
}

void Gyro::StickCenterCalibration()
{

    DBGLN("Gyro: Stick Center Calibration (Init)");
    stick_subtrim_cycles = 0;

    // initialize min, max & mid
    for (auto & ch_limit : temp_limits)
    {
        ch_limit.val.mid = GYRO_US_MID;
        ch_limit.val.min = GYRO_US_MID;
        ch_limit.val.max = GYRO_US_MID;
    }

    learn_state = GYRO_LEARN_SUBTRIMS;
}

void Gyro::StickLimitCalibration(bool done)
{
    DBGLN("Gyro: Stick Range Calibration (%s)", done ? "Complete" : "Started");

    if (done)
    {
        learn_state = GYRO_LEARN_LIMIT_DONE;
        // save the Range
        for (int ch = 0; ch < GYRO_MAX_CHANNELS; ch++)
        {
            const auto ch_info = gyroConfig->GetGyroChannel(ch);
            const auto output_mode = (gyro_output_channel_function_t)ch_info->val.output_mode;
            if (output_mode != FN_NONE && output_mode != FN_GYRO_GAIN && output_mode != FN_GYRO_MODE)
            {
                // Only moving surfaces
                const auto pwm_limits = &temp_limits[ch];
                DBGLN("Ch%d: Min: %d Max: %d Center: %d",
                      ch, (uint16_t)pwm_limits->val.min, (uint16_t)pwm_limits->val.max, (uint16_t)pwm_limits->val.mid);
                gyroConfig->SetPwmChannelLimitsRaw(ch, pwm_limits->raw);
            }
        }
        gyroConfig->Commit();
    }
    else
    {
        learn_state = GYRO_LEARN_LIMIT_START;
    }
}

bool Gyro::isStickCalibrationNeeded()
{
    bool isCalibrated = true;
    // DBGLN("IsStickCalibrationNeeded: Start");

    for (int ch = 0; ch < PWM_MAX_CHANNELS; ch++)
    {
        const auto ch_info = gyroConfig->GetGyroChannel(ch);
        const auto output_mode = (gyro_output_channel_function_t)ch_info->val.output_mode;
        const auto limits = gyroConfig->GetPwmChannelLimits(ch);
        if (output_mode != FN_NONE && output_mode != FN_GYRO_GAIN && output_mode != FN_GYRO_MODE)
        {
            // Only valid surfaces are checked
            if ((limits->val.max == GYRO_US_MAX && limits->val.min == GYRO_US_MIN) || // Default
                limits->val.max - limits->val.min < 30)
            {
                // Not moved the sticks much
                DBGLN("isStickCalibrationNeeded: Ch [%d] Not Calibrated", ch + 1);
                isCalibrated = false;
                sprintf(lastErrorText, "Ch%d Not Calibrated", ch + 1);
                break;
            }
        }
    }

    // DBGLN("IsStickCalibrationNeeded: All Calibrated");
    return !isCalibrated;
}

#endif

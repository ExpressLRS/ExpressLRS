#include "targets.h"

#if defined(PLATFORM_ESP32)
#include "ahrs.h"
#include "biasFilter.h"
#include "devGyro.h"
#include "filter.h"
#include "logging.h"

#define MAX_GYRO_DIFF 200
#define MAX_ACC_DIFF 500

#define SPIN_RATE_LIMIT 20             // Max 20 deg/sec
#define ATTITUDE_RESET_QUIET_TIME 250  // 250ms - gyro quiet period after after ACC out of range
#define ATTITUDE_RESET_GYRO_LIMIT 15   // 15 deg/sec - gyro limit for quiet period
#define ATTITUDE_RESET_KP_GAIN 10.0    // dcmKpGain value to use during attitude reset
#define ATTITUDE_RESET_ACTIVE_TIME 500 // 500ms - Time to wait for attitude to converge at high gain

#define invSqrt(x) (1.0f / sqrtf(x))

// Generic
// const char* mpuOrientationNames[8] = {
//    "FRONT(X+)", "BACK(X-)", "LEFT(Y+)", "RIGHT(Y-)", "UP(Z+)", "DOWN(Z-)", "WRONG", "WRONG"};

char **mpuOrientationNames;

// HR8EG
static const char *gyroRxOrientationsHR[8] =
    {"UART Up(X+)", "UART Dn(X-)", "Pins Up(Y+)", "Pins Dn(Y-)", "Lbl Up(Z+)", "Lbl Dn(Z-)", "WRONG", "WRONG"};
// RM
static const char *gyroRxOrientationsRM[8] =
    {"Pins Up(X+)", "Pins Dn(X-)", "V-Lbl Up(Y+)", "V-Lbl Dn(Y-)", "Lbl Up(Z+)", "Lbl Dn(Z-)", "WRONG", "WRONG"};

static int8_t orientationList[36][6] = {
    {3, 3, 3, 0, 0, 0}, 
    {3, 3, 3, 0, 0, 0}, 
    {1, 2, 0, 1, 1, 1}, 
    {1, 2, 0, -1, -1, 1}, 
    {2, 1, 0, 1, -1, 1}, 
    {2, 1, 0, -1, 1, 1},

    {3, 3, 3, 0, 0, 0},
    {3, 3, 3, 0, 0, 0},
    {1, 2, 0, 1, -1, -1},
    {1, 2, 0, -1, 1, -1},
    {2, 1, 0, 1, 1, -1},
    {2, 1, 0, -1, -1, -1},

    {0, 2, 1, 1, -1, 1},
    {0, 2, 1, -1, 1, 1},
    {3, 3, 3, 0, 0, 0},
    {3, 3, 3, 0, 0, 0},
    {2, 0, 1, 1, 1, 1},
    {2, 0, 1, -1, -1, 1},

    {0, 2, 1, 1, 1, -1},
    {0, 2, 1, -1, -1, -1},
    {3, 3, 3, 0, 0, 0},
    {3, 3, 3, 0, 0, 0},
    {2, 0, 1, 1, -1, -1},
    {2, 0, 1, -1, 1, -1},

    {0, 1, 2, 1, 1, 1},
    {0, 1, 2, -1, -1, 1},
    {1, 0, 2, 1, -1, 1},
    {1, 0, 2, -1, 1, 1},
    {3, 3, 3, 0, 0, 0},
    {3, 3, 3, 0, 0, 0},

    {0, 1, 2, 1, -1, -1},
    {0, 1, 2, -1, 1, -1},
    {1, 0, 2, 1, 1, -1},
    {1, 0, 2, -1, -1, -1},
    {3, 3, 3, 0, 0, 0},
    {3, 3, 3, 0, 0, 0}};

int8_t accLpfCutHz = 0;
int8_t gyroLpfCutHz = 0;

static LowpassFilter accFilter[3];
static LowpassFilter gyroFilter[3];

static unsigned long processingTimeUs = 0;
static unsigned long maxProcessingTimeUs = 0;

bool AHRS::initialize(IMU_Driver *driver)
{
    this->driver = driver;

    orientationIsWrong = true;
    initialized = false;
    memset(&calGyroOffsets, 0, sizeof(calGyroOffsets));
    memset(&calAccelOffets, 0, sizeof(calAccelOffets));
    read_errors = 0;

    mpuOrientationNames = (char **)(OPT_HAS_GYRO_MPU6050 ? gyroRxOrientationsHR : gyroRxOrientationsRM);

    return false;
}

/**
 * Trigger a ahrs to stop until restarted
 */
void AHRS::pause()
{
    initialized = false;
}

void AHRS::start()
{
    initialized = false;
    DBGLN("Gyro AHRS Start");

    if (!gyroConfig->GetGyroEnabled() || driver == nullptr)
        return; // not enabled

    driver->start();

    memcpy(&calAccelOffets, gyroConfig->GetAccelCalibration(), sizeof(rx_config_gyro_calibration_t));
    memcpy(&calGyroOffsets, gyroConfig->GetGyroCalibration(), sizeof(rx_config_gyro_calibration_t));
    DBGLN("Acc Offs:  x=%d,y=%d,z=%d", calAccelOffets.val.x, calAccelOffets.val.y, calAccelOffets.val.z);
    DBGLN("Gyro Offs:  x=%d,y=%d,z=%d", calGyroOffsets.val.x, calGyroOffsets.val.y, calGyroOffsets.val.z);

    setupOrientation();

    read_errors = 0; // Reset Errors

    // Reload Madgwick Configuration
    const rx_config_gyro_PID_t *madgwickPI = gyroConfig->GetGyroPID(GYRO_PID_GROUP_MADGWICK, GYRO_AXIS_ROLL);
    AHRS_KP = (float)madgwickPI->val.p / 10;
    AHRS_KI = (float)madgwickPI->val.i / 10;

    accLpfCutHz = gyroLpfCutHz = madgwickPI->val.d;

    DBGLN("Setting Madgwick kP=%f, kI=%f", AHRS_KP, AHRS_KI);
    DBGLN("LPF Gyro Settings %d HZ", accLpfCutHz);

    gyroBias.Initialise(driver->gyroSampleRate);

    for (int axis = 0; axis < 3; axis++)
    {
        if (accLpfCutHz > 0)
        {
            accFilter[axis].init(LPF_PT1, accLpfCutHz, driver->gyroSampleRate, 0);
        }
        if (gyroLpfCutHz > 0)
        {
            gyroFilter[axis].init(LPF_PT1, gyroLpfCutHz, driver->gyroSampleRate, 0);
        }
    }

    initialized = true;
}

uint8_t AHRS::event()
{
    return DURATION_IGNORE;
}

bool AHRS::isRunning()
{
    return initialized && !orientationIsWrong && !isCalibrating;
}

/**
 * This method is used instead of mpu->dmpGetYawPitchRoll() as that method has
 * issues when gravity switches at high pitch angles.
 */
static void GetRollPitchYaw(float *data_rpy, Quaternion *q, VectorFloat *gravity)
{
    /* using Gravity Vector */

    // yaw: (about Z axis)
    data_rpy[2] = atan2(2 * q->x * q->y - 2 * q->w * q->z, 2 * q->w * q->w + 2 * q->x * q->x - 1);
    // pitch: (nose up/down, about Y axis)
    data_rpy[1] = atan2(gravity->x, sqrt(gravity->y * gravity->y + gravity->z * gravity->z));
    // roll: (tilt left/right, about X axis)
    data_rpy[0] = atan2(gravity->y, gravity->z);

    /* using Quartilion */
    /*
    // yaw: (about Z axis)
    data_rpy[2] = -atan2((q->x * q->y + q->w * q->z), 0.5 - (q->y * q->y + q->z * q->z));
    // pitch: (nose up/down, about Y axis)
    data_rpy[1] = -asin(2.0 * (q->w * q->y - q->x * q->z));
    // roll: (tilt left/right, about X axis)
    data_rpy[0] = atan2((q->w * q->x + q->y * q->z), 0.5 - (q->x * q->x + q->y * q->y));
    */

    // NOTE: This is buggy at high pitch angles when gravity flips
    // if (gravity -> z < 0) {
    //     if(data_rpy[1] > 0) {
    //         data_rpy[1] = PI - data_rpy[1];
    //     } else {
    //         data_rpy[1] = -PI - data_rpy[1];
    //     }
    // }
}

static void GetGravity(VectorFloat *v, Quaternion *q)
{
    v->x = 2 * (q->x * q->z - q->w * q->y);
    v->y = 2 * (q->w * q->x + q->y * q->z);
    v->z = q->w * q->w - q->x * q->x - q->y * q->y + q->z * q->z;
}

void AHRS::applyOrientation(VectorInt16 *v)
{
    // take care of the orientation of the sensor in the model
    float t[3];
    t[0] = v->x;
    t[1] = v->y;
    t[2] = v->z;

    v->x = t[orientationX] * orientationSignX;
    v->y = t[orientationY] * orientationSignY;
    v->z = t[orientationZ] * orientationSignZ;
}

// Calculate the KpGain to use.
// When disarmed after initial boot, the scaling is set to 10.0 for the first 20 seconds to speed up initial convergence.
// After disarming we want to quickly reestablish convergence to deal with the attitude estimation being incorrect due to a crash.
//   - wait for a 250ms period of low gyro activity to ensure the craft is not moving
//   - use a large dcmKpGain value for 500ms to allow the attitude estimate to quickly converge
//   - reset the gain back to the standard setting
static float imuCalcKpGain(bool useAcc, const VectorFloat gyro, bool *quartilionReset)
{
    static uint8_t state = 0;
    static long gyroQuietPeriodTimeEnd_us = 0;
    static long attitudeResetTimeEnd_us = 0;

    float ret = 1.0;
    *quartilionReset = false;

    long currentTimeUs = micros();
    // If gyro activity exceeds the threshold then restart the quiet period.
    // Also, if the attitude reset has been complete and there is subsequent gyro activity then
    // start the reset cycle again.

    if ((fabsf(gyro.x) > ATTITUDE_RESET_GYRO_LIMIT) || (fabsf(gyro.y) > ATTITUDE_RESET_GYRO_LIMIT) || (fabsf(gyro.z) > ATTITUDE_RESET_GYRO_LIMIT) || (!useAcc))
    {

        gyroQuietPeriodTimeEnd_us = currentTimeUs + ATTITUDE_RESET_QUIET_TIME * 1000;
        state = 1;
    }

    if (state == 1)
    { // In Quiet Period
        if (currentTimeUs >= gyroQuietPeriodTimeEnd_us)
        {
            // Start the high gain period to bring the estimation into convergence
            attitudeResetTimeEnd_us = currentTimeUs + ATTITUDE_RESET_ACTIVE_TIME * 1000;
            gyroQuietPeriodTimeEnd_us = 0;
            state = 2;
            //*quartilionReset = true; // Reset Quarterion, so will think that is level, but will fix itself
        }
    }

    if (state == 2)
    { // In Attitude Reset
        if (currentTimeUs >= attitudeResetTimeEnd_us)
        {
            gyroQuietPeriodTimeEnd_us = 0;
            attitudeResetTimeEnd_us = 0;
            state = 0;
        }
        else
        {
            // Still in Attitude Reset
            ret = 10.0; // To converge faster
        }
    }

    return ret;
}

// For Debugging when log is on
static float totalAccG = 0;
static float kpFactor = 1.0;
boolean AHRS::isAccelHeathty(VectorFloat acc, float *kp, float *ki)
{
#define KP1_LOW_LIMIT 0.90  // 0.975
#define KP1_HIGH_LIMIT 1.10 // 1.025

    float tmp = sq(acc.x) + sq(acc.y) + sq(acc.z);
    float totalAcc = sqrtf(tmp);

    totalAccG = totalAcc;

    if ((totalAcc > KP1_LOW_LIMIT) && (totalAcc < KP1_HIGH_LIMIT))
    { // When total acceleration is within some limits (close 1g)
        *kp = AHRS_KP;
        *ki = AHRS_KI;
    }
    else
    { // Aceleration out of range
        *kp = 0;
        *ki = 0;
        return false;
    }

    return true;
}

bool AHRS::readAndUpdate()
{
    if (orientationIsWrong)
        return false;

    static auto last = micros(); // Behaves like Global
    const auto now = micros();

    // Regular READ
    if (!driver->rawRead(&v_accel.x, &v_accel.y, &v_accel.z,
                         &v_gyro.x, &v_gyro.y, &v_gyro.z))
    {
        read_errors++;
        return false;
    }

    // Apply Calibration offsets
    v_accel.x -= calAccelOffets.val.x;
    v_accel.y -= calAccelOffets.val.y;
    v_accel.z -= calAccelOffets.val.z;

    v_gyro.x -= calGyroOffsets.val.x;
    v_gyro.y -= calGyroOffsets.val.y;
    v_gyro.z -= calGyroOffsets.val.z;

    applyOrientation(&v_accel);
    applyOrientation(&v_gyro);

    if (accLpfCutHz > 0)
    {
        v_accel.x = accFilter[GYRO_AXIS_ROLL].apply(v_accel.x);
        v_accel.y = accFilter[GYRO_AXIS_PITCH].apply(v_accel.y);
        v_accel.z = accFilter[GYRO_AXIS_YAW].apply(v_accel.z);
    }

    if (gyroLpfCutHz > 0)
    {
        v_gyro.x = gyroFilter[GYRO_AXIS_ROLL].apply(v_gyro.x);
        v_gyro.y = gyroFilter[GYRO_AXIS_PITCH].apply(v_gyro.y);
        v_gyro.z = gyroFilter[GYRO_AXIS_YAW].apply(v_gyro.z);
    }

    // use Mahoney filter
    float deltat = ((float)(now - last)) * 1.0e-6f; // seconds since last update
    last = now;

    // Get Gyro in Degrees/sec
    VectorFloat gDeg;
    float scaleDeg = driver->gyroScaleDeg;
    gDeg.x = ((float)v_gyro.x) * scaleDeg;
    gDeg.y = ((float)v_gyro.y) * scaleDeg;
    gDeg.z = ((float)v_gyro.z) * scaleDeg;

    // Get Acc in Gs
    VectorFloat accG;
    float scaleG = driver->accScaleG;
    accG.x = ((float)v_accel.x) * scaleG;
    accG.y = ((float)v_accel.y) * scaleG;
    accG.z = ((float)v_accel.z) * scaleG;

    gyroBias.Update(gDeg);

    float kp, ki;
    bool useAcc = isAccelHeathty(accG, &kp, &ki);

    bool quartilionReset;
    kpFactor = imuCalcKpGain(useAcc, gDeg, &quartilionReset);
    kp = kp * kpFactor;

    if (quartilionReset)
    {
        q.reset();
    }

    Mahony_update(useAcc, accG.x, accG.y, accG.z,
                  radians(gDeg.x), radians(gDeg.y), radians(gDeg.z),
                  &q, deltat, kp, ki);

    GetGravity(&gravity, &q);
    GetRollPitchYaw(angle_rpy, &q, &gravity);

    // Gyro in  rad/s
    gyro_rpy[0] = radians(gDeg.x); // Roll
    gyro_rpy[1] = radians(gDeg.y); // Pitch
    gyro_rpy[2] = radians(gDeg.z); // Yaw

    // Acc in  Gs
    acc_rpy[0] = accG.x; // Roll
    acc_rpy[1] = accG.y; // Pitch
    acc_rpy[2] = accG.z; // Yaw

#ifdef DEBUG_GYRO_STATS
    processingTimeUs = micros() - now;
    maxProcessingTimeUs = max(maxProcessingTimeUs, processingTimeUs);
    printGyroStats(now);
#endif

    lastGyroUpdate_us = now;
    return true;
}

void AHRS::setupOrientation()
{
    static uint8_t REV_IDX[] = {1, 0, 3, 2, 5, 4, 6};

    uint8_t idx;
    orientationIsWrong = false;
    imuOrientationH = gyroConfig->GetGyroOrientationH();
    imuOrientationV = gyroConfig->GetGyroOrientationV();

    if (imuOrientationH > 5 || imuOrientationV > 5)
    {
        orientationIsWrong = true;
        DBGLN("Orientation is WRONG");
        return;
    }

    DBGLN("Orientation H/top: %s", mpuOrientationNames[imuOrientationH]);
    DBGLN("Orientation V/nose down: %s", mpuOrientationNames[imuOrientationV]);

    // Reverse the Gravity orientation index for vertical, since is nose DOWN instead of UP
    // but logic expect the face to the front, and not the tail
    imuOrientationV = REV_IDX[imuOrientationV]; 

    idx = imuOrientationH * 6 + imuOrientationV; // into a number in range 0/35
    if (orientationList[idx][3] == 0)
    { // check that combination H and V is valid
        orientationIsWrong = true;
        return;
    }
    orientationX = orientationList[idx][0]; // orientation list contains e.g. 3,2,1,-1,1,1 (first are the index to map, last 3 the sign)
    orientationY = orientationList[idx][1];
    orientationZ = orientationList[idx][2];
    orientationSignX = orientationList[idx][3];
    orientationSignY = orientationList[idx][4];
    orientationSignZ = orientationList[idx][5];
}

void AHRS::findGravity(int32_t ax, int32_t ay, int32_t az, uint8_t &idx)
{
    // find the index and sign of gravity
    //      idx:  0=X, 1=Y, 2=Z;
    //      sign: 1=gravity is the opposite (normally Z axis is up and give 1)

    float oneG_70percent = driver->acc1G_adc * 0.7f;

    if ((float)ax > oneG_70percent)
    {
        idx = 0;
    }
    else if ((float)ax < -oneG_70percent)
    {
        idx = 1;
    }
    else if ((float)ay > oneG_70percent)
    {
        idx = 2;
    }
    else if ((float)ay < -oneG_70percent)
    {
        idx = 3;
    }
    else if ((float)az > oneG_70percent)
    {
        idx = 4;
    }
    else if ((float)az < -oneG_70percent)
    {
        idx = 5;
    }
    else
    {
        idx = 6;
    };

    DBGLN("findGravty(): ax=%d  ay=%d  az=%d  yawIdx=%d  scale=%f", ax, ay, az, idx, driver->acc1G_adc);
}

uint8_t AHRS::readAndGetGravity()
{ // return index of orientation; return 6 in case of error
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    uint8_t idx = 6;

    uint8_t i = 3;
    do
    {
        if (driver->rawRead(&ax, &ay, &az, &gx, &gy, &gz))
        {
            findGravity(ax, ay, az, idx);
            break;
        }
        i = -1;
    } while (i > 0);
    return idx;
}

void AHRS::OrientationHorizontalExecute() //
{
    orientationIsWrong = true;
    imuOrientationH = 6;
    imuOrientationV = 6;

    DBGLN("Horizontal Detection...");
    uint8_t idx = readAndGetGravity(); // // read the Acc and detect which face is on the upper side
    if (idx > 5)
    {
        DBGLN("Error during horizontal orientation: direction of gravity has not been found");
        return;
    }
    DBGLN("Upper face RX (when model is horizontal) is %s", mpuOrientationNames[idx]);
    imuOrientationH = idx; // save the orientationH

    // Run the calibration, but not saving the offsets
    calibrateAccel(8, &calAccelOffets);
    calibrateGyro(8, &calGyroOffsets);
}

void AHRS::OrientationVerticalExecute()
{
    imuOrientationV = 6;
    DBGLN("Vertical Detection...");
    uint8_t idx = readAndGetGravity(); // // read the Acc and detect which face is on the upper side
    if (idx > 5)
    {
        DBGLN("Error during vertical orientation: direction of gravity has not been found");
    }
    DBGLN("Face Vert/Tail (with nose down) is %s", mpuOrientationNames[idx]);
    imuOrientationV = idx; // save the orientationV

    gyroConfig->SetGyroOrientation(imuOrientationH, imuOrientationV);

    // Save the Calibration
    gyroConfig->SetAccelCalibration(calAccelOffets.val.x, calAccelOffets.val.y, calAccelOffets.val.z);
    gyroConfig->SetGyroCalibration(calGyroOffsets.val.x, calGyroOffsets.val.y, calGyroOffsets.val.z);

}

//--------------------------------------------------------------------------------------------------
// Mahony scheme uses proportional and integral filtering on
// the error between estimated reference vector (gravity) and measured one.
// Madgwick's implementation of Mayhony's AHRS algorithm.
// See: http://www.x-io.co.uk/node/8#open_source_ahrs_and_imu_algorithms
//
// Date      Author      Notes
// 29/09/2011 SOH Madgwick    Initial release
// 02/10/2011 SOH Madgwick  Optimised for reduced CPU load
// last update 07/09/2020 SJR minor edits
//--------------------------------------------------------------------------------------------------
// IMU algorithm update

// currently ax, ay, az are in Gs and gx,gy,gz are in rad/sec.
void AHRS::Mahony_update(bool useAcc,
                         float ax, float ay, float az,
                         float gx, float gy, float gz,
                         Quaternion *q,
                         float deltat, float kp, float ki)
{
    static float ix = 0.0, iy = 0.0, iz = 0.0; // integral feedback terms
    float ex = 0.0f, ey = 0.0f, ez = 0.0f;     // error terms

    // Compute feedback only if accelerometer measurement valid (avoids NaN in accelerometer normalisation)
    float tmpAcc = sq(ax) + sq(ay) + sq(az);
    if (useAcc && tmpAcc > 0.01f)
    { // Original tmpAcc > 0.0, Rotorflight have it as > 0.01f
        // Normalise accelerometer (assumed to measure the direction of gravity in body frame)
        float recipNorm = invSqrt(tmpAcc);
        ax *= recipNorm;
        ay *= recipNorm;
        az *= recipNorm;

        // Estimated direction of gravity in the body frame (factor of two divided out)
        const float vx = q->x * q->z - q->w * q->y;
        const float vy = q->w * q->x + q->y * q->z;
        const float vz = q->w * q->w - 0.5f + q->z * q->z;

        // Error is cross product between estimated and measured direction of gravity in body frame
        // (half the actual magnitude)
        ex = (ay * vz - az * vy);
        ey = (az * vx - ax * vz);
        ez = (ax * vy - ay * vx);
    }

    // Compute and apply to gyro term the integral feedback, if enabled
    if (ki > 0.0f)
    {
        // Calculate general spin rate (rad/s)
        const float spin_rate = sqrtf(sq(gx) + sq(gy) + sq(gz));

        // Stop integrating if spinning beyond the certain limit
        if (spin_rate < radians(SPIN_RATE_LIMIT))
        {
            ix += ki * ex * deltat; // integral error scaled by Ki
            iy += ki * ey * deltat;
            iz += ki * ez * deltat;
        }
    }
    else
    {
        ix = 0;
        iy = 0;
        iz = 0;
    }

    // Apply proportional and Integral feedback to gyro term
    gx += kp * ex + ix;
    gy += kp * ey + iy;
    gz += kp * ez + iz;

    // Integrate rate of change of quaternion, q cross gyro term
    deltat = 0.5 * deltat; // pre-multiply common factors
    gx *= deltat;
    gy *= deltat;
    gz *= deltat;

    const float tq_w = q->w;
    const float tq_x = q->x;
    const float tq_y = q->y;

    q->w += (-tq_x * gx - tq_y * gy - q->z * gz);
    q->x += (+tq_w * gx + tq_y * gz - q->z * gy);
    q->y += (+tq_w * gy - tq_x * gz + q->z * gx);
    q->z += (+tq_w * gz + tq_x * gy - tq_y * gx);

    // renormalise quaternion
    tmpAcc = sq(q->w) + sq(q->x) + sq(q->y) + sq(q->z);
    if (tmpAcc == 0)
        return; // Prevent NaN

    float recipNorm = invSqrt(tmpAcc);
    q->w = q->w * recipNorm;
    q->x = q->x * recipNorm;
    q->y = q->y * recipNorm;
    q->z = q->z * recipNorm;
}

bool AHRS::calibrateGyro(int8_t loops, rx_config_gyro_calibration_t *offsets)
{
#define ACCEL_NUM_AVG_SAMPLES 300

    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    int32_t gxAccum, gyAccum, gzAccum, gxMin, gxMax, gyMin, gyMax, gzMin, gzMax;
    gxAccum = gyAccum = gzAccum = 0;
    gxMin = gyMin = gzMin = 60000;
    gxMax = gyMax = gzMax = -60000;

    int16_t errors = 0;

    DBGLN("Stating Gyro Calibration..");
    isCalibrating = true;

    for (int inx = 0; inx < ACCEL_NUM_AVG_SAMPLES; inx++)
    {
        DBG(".");
        int c = 0;
        while (!driver->isDataReady() && c++ < 20)
        {
            delayMicroseconds(50);
        }
        if (!driver->rawRead(&ax, &ay, &az, &gx, &gy, &gz))
        {
            errors++;
            continue;
        }

        gxAccum += (int32_t)gx;
        gyAccum += (int32_t)gy;
        gzAccum += (int32_t)gz;

        if (gx < gxMin)
            gxMin = gx;
        if (gy < gyMin)
            gyMin = gy;
        if (gz < gzMin)
            gzMin = gz;
        if (gx > gxMax)
            gxMax = gx;
        if (gy > gyMax)
            gyMax = gy;
        if (gz > gzMax)
            gzMax = gz;
    }

    DBGLN("\nGyro Calibration completed..");

    DBGLN("Calibration: gyro differences: x=%d (%d/%d) y=%d (%d/%d) z=%d (%d/%d)",
          gxMax - gxMin, gxMax, gxMin,
          gxMax - gxMin, gxMax, gxMin,
          gzMax - gzMin, gzMax, gzMin);

    if (((gxMax - gxMin) > MAX_GYRO_DIFF) or ((gyMax - gyMin) > MAX_GYRO_DIFF) or ((gzMax - gzMin) > MAX_GYRO_DIFF))
    {
        DBGLN("Error in IMU calibration: to much variations in the gyro values");
        isCalibrating = false;
        return false;
    }

    if (errors > 50)
    {
        DBGLN("Too many read errors during calibration  (Errors=%d)", errors);
        isCalibrating = false;
        return false;
    }

    gxAccum /= (ACCEL_NUM_AVG_SAMPLES - errors);
    gyAccum /= (ACCEL_NUM_AVG_SAMPLES - errors);
    gzAccum /= (ACCEL_NUM_AVG_SAMPLES - errors);

    offsets->val.x = (int16_t)(gxAccum);
    offsets->val.y = (int16_t)(gyAccum);
    offsets->val.z = (int16_t)(gzAccum);

    DBGLN("Gyr Offs:  x=%d,y=%d,z=%d", offsets->val.x, offsets->val.y, offsets->val.z);
    isCalibrating = false;
    return true;
}

bool AHRS::calibrateAccel(int8_t loops, rx_config_gyro_calibration_t *offsets)
{
    int16_t ax, ay, az;
    int16_t gx, gy, gz;
    int32_t axAccum, ayAccum, azAccum, axMin, axMax, ayMin, ayMax, azMin, azMax;
    int16_t errors = 0;

    axAccum = ayAccum = azAccum = 0;
    axMin = ayMin = azMin = 60000;
    axMax = ayMax = azMax = -60000;

    DBGLN("Stating Accelerometer Calibration. OrientationH: %s", mpuOrientationNames[imuOrientationH]);
    isCalibrating = true;

    for (int inx = 0; inx < ACCEL_NUM_AVG_SAMPLES; inx++)
    {
        DBG(".");
        int c = 0;
        while (!driver->isDataReady() && c++ < 20)
        {
            delayMicroseconds(50);
        }
        if (!driver->rawRead(&ax, &ay, &az, &gx, &gy, &gz))
        {
            errors++;
            continue;
        }

        // Remove Gravity
        const float acc1G = driver->acc1G_adc;
        switch (imuOrientationH)
        {
        case 0:
            ax -= acc1G;
            break;
        case 1:
            ax += acc1G;
            break;
        case 2:
            ay -= acc1G;
            break;
        case 3:
            ay += acc1G;
            break;
        case 4:
            az -= acc1G;
            break;
        case 5:
            az += acc1G;
            break;
        }

        axAccum += (int32_t)ax;
        ayAccum += (int32_t)ay;
        azAccum += (int32_t)az;

        if (ax < axMin)
            axMin = ax;
        if (ay < ayMin)
            ayMin = ay;
        if (az < azMin)
            azMin = az;
        if (ax > axMax)
            axMax = ax;
        if (ay > ayMax)
            ayMax = ay;
        if (az > azMax)
            azMax = az;
    }

    DBGLN("\nAccelerometer Calibration completed..");

    DBGLN("Calibration: acceleration differences: x=%d (%d/%d) y=%d (%d/%d) z=%d (%d/%d)",
          axMax - axMin, axMax, axMin,
          axMax - axMin, axMax, axMin,
          azMax - azMin, azMax, azMin);

    // here we know the Acc but still will reject the measurement if noise is to big

    if (((axMax - axMin) > MAX_ACC_DIFF) or ((ayMax - ayMin) > MAX_ACC_DIFF) or ((azMax - azMin) > MAX_ACC_DIFF))
    {
        DBGLN("Error in IMU calibration: to much variations in the acceleration values: x=%d y=%d z=%d", axMax - axMin, ayMax - ayMin, azMax - azMin);
        isCalibrating = false;
        return false;
    }

    if (errors > 50)
    {
        DBGLN("Too many read errors during calibration  (Errors=%d)", errors);
        isCalibrating = false;
        return false;
    }

    axAccum /= (ACCEL_NUM_AVG_SAMPLES - errors);
    ayAccum /= (ACCEL_NUM_AVG_SAMPLES - errors);
    azAccum /= (ACCEL_NUM_AVG_SAMPLES - errors);

    // we store the values
    offsets->val.x = (int16_t)(axAccum);
    offsets->val.y = (int16_t)(ayAccum);
    offsets->val.z = (int16_t)(azAccum);

    DBGLN("Acc Offs:  x=%d,y=%d,z=%d", offsets->val.x, offsets->val.y, offsets->val.z);
    isCalibrating = false;
    return true;
}

void AHRS::calibrate(bool save)
{
    // Run the calibration
    calibrateAccel(8, &calAccelOffets);
    calibrateGyro(8, &calGyroOffsets);

    if (!save)
        return;

    gyroConfig->SetAccelCalibration(
        calAccelOffets.val.x,
        calAccelOffets.val.y,
        calAccelOffets.val.z);
    gyroConfig->SetGyroCalibration(
        calGyroOffsets.val.x,
        calGyroOffsets.val.y,
        calGyroOffsets.val.z);
}

int AHRS::tick()
{
    // Behaves like Global
    static unsigned long next_check_us = micros();

    if (!initialized && isCalibrating)
    {
        // DBGLN("Gyro not Ready or Calibrating.. return in 1s");
        return 1000; // come back in 1000 ms if not initialized
    }

    // Returned earlier than the ODR Period??
    const auto now = micros();
    if (now < next_check_us)
    {
        return DURATION_IMMEDIATELY;
    }

    // Do we have Gyro data Available ??
    if (!driver->isDataReady())
    {
        return DURATION_IMMEDIATELY;
    }

    // Update next_check_us. We use the theoretical GyroSampleRate to see how
    // many uS do we have to wait to try to read again the gyro
    next_check_us = now + driver->period_us;

    readAndUpdate();

    // Loop again as fast as we can, the refresh rate of the gyro ODR
    return DURATION_IMMEDIATELY;
}

#ifdef DEBUG_GYRO_STATS
/**
 * For debugging print useful gyro state
 */
void AHRS::printGyroStats(long nowMicros)
{
    static long last_gyro_stats_time = 0;
    static int update_rate = 0;

    if (millis() - last_gyro_stats_time < 500)
        return;

    // Calculate gyro update rate in HZ
    int current_rate = 1.0 / ((nowMicros - lastGyroUpdate_us) / 1000000.0);
    update_rate = (update_rate + current_rate) / 2; // Average

    //char rate_str[15];
    //sprintf(rate_str, "%4d", update_rate);

    char pitch_str[15];
    sprintf(pitch_str, "%6.2f", degrees(angle_rpy[1]));
    char roll_str[15];
    sprintf(roll_str, "%6.2f", degrees(angle_rpy[0]));
    char yaw_str[15];
    sprintf(yaw_str, "%6.2f", degrees(angle_rpy[2]));

    char gyro_x[15];
    sprintf(gyro_x, "%6.3f", (double)v_gyro.x * driver->gyroScaleDeg);
    char gyro_y[15];
    sprintf(gyro_y, "%6.3f", (double)v_gyro.y * driver->gyroScaleDeg);
    char gyro_z[15];
    sprintf(gyro_z, "%6.3f", (double)v_gyro.z * driver->gyroScaleDeg);

    char accel_x[15];
    sprintf(accel_x, "%6.3f", (double)v_accel.x * driver->accScaleG);
    char accel_y[15];
    sprintf(accel_y, "%6.3f", (double)v_accel.y * driver->accScaleG);
    char accel_z[15];
    sprintf(accel_z, "%6.3f", (double)v_accel.z * driver->accScaleG);

    char gravity_x[15];
    sprintf(gravity_x, "%4.3f", gravity.x);
    char gravity_y[15];
    sprintf(gravity_y, "%4.3f", gravity.y);
    char gravity_z[15];
    sprintf(gravity_z, "%4.3f", gravity.z);

    VectorFloat o = gyroBias.getOffsets();
    char bias_x[15];
    sprintf(bias_x, "%4.3f", o.x);
    char bias_y[15];
    sprintf(bias_y, "%4.3f", o.y);
    char bias_z[15];
    sprintf(bias_z, "%4.3f", o.z);

    // Uncomment lines needed for debugging
    DBGLN("**********************");
    DBGLN("Refresh: %d HZ, Period=%d uS, Theory period = %d uS", update_rate, nowMicros - lastGyroUpdate_us, driver->period_us);
    DBGLN("Execution duration: Last %d uS, Max %d uS", processingTimeUs, maxProcessingTimeUs);
    DBGLN("interrupt non_ready_errors=%d  read_errors=%d ", driver->non_ready_errors, read_errors);
    DBGLN("Pitch:%s Roll:%s Yaw:%s", pitch_str, roll_str, yaw_str);
    DBGLN("Q       (w: %f, x: %f, y: %f, z: %f)", q.w, q.x, q.y, q.z);
    DBGLN("Gyro    (x: %s, y: %s, z: %s)", gyro_x, gyro_y, gyro_z);
    DBGLN("GBias   (x: %s, y: %s, z: %s)", bias_x, bias_y, bias_z);
    DBGLN("Accel   (x: %s, y: %s, z: %s)", accel_x, accel_y, accel_z);
    DBGLN("Gravity (x: %s, y: %s, z: %s)", gravity_x, gravity_y, gravity_z);
    DBGLN("TotalAccHeath (%f)   Kp=(%f) Ki=(%f) ", totalAccG, AHRS_KP * kpFactor, AHRS_KI);

    last_gyro_stats_time = millis();
}
#endif

#endif

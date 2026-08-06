#include "targets.h"

#if defined(GYRO_SUPPORT)
#include "gyro.h"
//#include "utils.h"
#include "logging.h"
#include "elrs_eeprom.h" // only needed to satisfy PIO
#include "config.h"
#include "CRSFRouter.h"


#include "drivers/mpu6050.h"
#include "drivers/lsm6dXX.h"


extern boolean i2c_enabled;
extern boolean spi_enabled;

Gyro gyro = Gyro();
AHRS ahrs = AHRS();

static IMU_Driver *driver = nullptr;

static bool initialize()
{
    if (! OPT_HAS_GYRO) return false;

    driver = nullptr;

    // I2C Gyros
    if (i2c_enabled)
    {
        if (driver == nullptr  && OPT_HAS_GYRO_MPU6050)
        {
            driver = new IMU_MPU6050();
            if (driver->initialize()) {
                DBGLN("devGyro.init(): Detected MPU6050 Gyro");
            } else {
                driver = nullptr;
            } 
        }
    }

    // SPI Gyros
    if(spi_enabled) {
        if (driver == nullptr && OPT_HAS_GYRO_LSM6DXX)
        {
            driver = new IMU_LSM6DXX_SPI();
            if (driver->initialize()) {
                DBGLN("devGyro.init(): Detected LSM6DXX Gyro");
            } else {
                driver=nullptr;
            }
        }
    } // if SPI

    if (driver==nullptr) {
         DBGLN("devGyro.init(): Gyro Not Detected");
    }

    // Call Init even when driver is null to disable other parts looking 
    // if gyro is OFF
    ahrs.initialize(driver);
    gyro.init(&ahrs);

    return driver!=nullptr;
}

static bool gyro_detect() {
    return driver != nullptr;
}

static int start()
{
    DBGLN("devGyro.start()");
    if (!gyro_detect()) {
        DBGLN("Gyro initialization failed");
        return DURATION_NEVER;
    }
    gyro.start();
    ahrs.start();
    return DURATION_IMMEDIATELY; // Call timeout() immediately;
}

extern const char* STR_gyroMode[];
static void send_telemetry()
{
    // Get yaw/pitch/roll in decidegrees and convert to uint16_t
#if 0    
    uint16_t rpy16[3] = {0};
    rpy16[GYRO_AXIS_ROLL]   = (uint16_t)(gyro.angle_rpy[GYRO_AXIS_ROLL] * 1800 / M_PI);
    rpy16[GYRO_AXIS_PITCH]  = (uint16_t)(gyro.angle_rpy[GYRO_AXIS_PITCH] * 1800 / M_PI);
    rpy16[GYRO_AXIS_YAW]    = (uint16_t)(gyro.angle_rpy[GYRO_AXIS_YAW] * 1800 / M_PI);
    
    CRSF_MK_FRAME_T(crsf_sensor_attitude_t)
    crsfAttitude = {0};
    crsfAttitude.p.pitch = htobe16(decidegrees2Radians10000(rpy16[GYRO_AXIS_PITCH]));
    crsfAttitude.p.roll = htobe16(decidegrees2Radians10000(rpy16[GYRO_AXIS_ROLL]));
    crsfAttitude.p.yaw = htobe16(decidegrees2Radians10000(rpy16[GYRO_AXIS_YAW]));
#endif
    CRSF_MK_FRAME_T(crsf_sensor_attitude_t)
    crsfAttitude = {0};
    crsfAttitude.p.pitch = htobe16(ahrs.angle_rpy[GYRO_AXIS_PITCH] * 10000);
    crsfAttitude.p.roll = htobe16(ahrs.angle_rpy[GYRO_AXIS_ROLL]   * 10000);
    crsfAttitude.p.yaw = htobe16(ahrs.angle_rpy[GYRO_AXIS_YAW]     * 10000);


    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfAttitude, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_attitude_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfAttitude.h);

    // Gyro
    CRSF_MK_FRAME_T(crsf_sensor_gyro_t)
    crsfGyro = {0};

    crsfGyro.p.sample_time = htobe32(millis());
    crsfGyro.p.gyro_temp = 0;
    
    // In Gs
    crsfGyro.p.acc_x = htobe16(ahrs.acc_rpy[GYRO_AXIS_PITCH] * 1000);
    crsfGyro.p.acc_y = htobe16(ahrs.acc_rpy[GYRO_AXIS_ROLL]  * 1000);
    crsfGyro.p.acc_z = htobe16(ahrs.acc_rpy[GYRO_AXIS_YAW] * 1000);

    // From Radians/s to Deg/s
    crsfGyro.p.gyro_x = htobe16(degrees(ahrs.gyro_rpy[GYRO_AXIS_PITCH])); 
    crsfGyro.p.gyro_y = htobe16(degrees(ahrs.gyro_rpy[GYRO_AXIS_ROLL]));
    crsfGyro.p.gyro_z = htobe16(degrees(ahrs.gyro_rpy[GYRO_AXIS_YAW]));

    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfGyro, CRSF_FRAMETYPE_GYRO, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gyro_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfGyro.h);

    // Flight Mode
    CRSF_MK_FRAME_T(crsf_flight_mode_t)
    crsfFlightMode = {0};

    strcpy(crsfFlightMode.p.flight_mode, STR_gyroMode[gyro.gyro_mode]);

    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfFlightMode, CRSF_FRAMETYPE_FLIGHT_MODE, CRSF_FRAME_SIZE(sizeof(crsf_flight_mode_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_CRSF_TRANSMITTER, &crsfFlightMode.h);
}

static int timeout()
{
    static long lastTelSent_ms = 0; // Last time we sent telemetry (millis)

    if (!ahrs.isRunning()) return 1000; // 1 seconds wait if not running

    if ((millis() - lastTelSent_ms) > 500 ) { // 500 ms (1/2s) cycle
        lastTelSent_ms = millis();
        send_telemetry();
    }

    return ahrs.tick();
}

static int event()
{
    DBGLN("deGyro.Event()");
    ahrs.event();
    gyro.event();
    return DURATION_IGNORE;
}

device_t Gyro_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout,
    .subscribe = EVENT_CONFIG_GYRO_CHANGED,
};

#endif
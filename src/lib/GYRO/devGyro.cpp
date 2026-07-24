#include "targets.h"

#if defined(PLATFORM_ESP32)
#include "gyro.h"
#include "logging.h"

#include "CRSFRouter.h"
#include "device.h"
#include "gyro_config.h"

#include "drivers/lsm6dXX.h"
#include "drivers/mpu6050.h"
#include "drivers/sc7u22.h"


extern boolean i2c_enabled;
extern boolean spi_enabled;

Gyro gyro = Gyro();
AHRS ahrs = AHRS();
GyroConfig *gyroConfig;

static IMU_Driver *driver = nullptr;

bool gyroDetected()
{
    return driver != nullptr;
}

static bool initialize()
{
    if (!OPT_HAS_GYRO)
        return false;

    driver = nullptr;

    // I2C Gyros
    if (i2c_enabled || (hardware_pin(HARDWARE_gyro_scl) != UNDEF_PIN && hardware_pin(HARDWARE_gyro_sda) != UNDEF_PIN))
    {
        if (driver == nullptr && OPT_HAS_GYRO_MPU6050)
        {
            driver = new IMU_MPU6050();
            if (!driver->initialize())
            {
                delete driver;
                driver = nullptr;
            }
        }

        if (driver == nullptr && OPT_HAS_GYRO_SC7U22)
        {
            driver = new IMU_SC7U22();
            if (driver->initialize()) {
                DBGLN("devGyro.init(): Detected SC7U22 Gyro");
            } else {
                driver = nullptr;
            }
        }
    }

    // SPI Gyros
    if (spi_enabled)
    {
        if (driver == nullptr && OPT_HAS_GYRO_LSM6DXX)
        {
            driver = new IMU_LSM6DXX_SPI();
            if (!driver->initialize())
            {
                delete driver;
                driver = nullptr;
            }
        }
    } // if SPI

    if (driver == nullptr)
    {
        DBGLN("devGyro: Gyro Not Detected");
        return false;
    }

    ahrs.initialize(driver);
    gyro.init(&ahrs);

    gyroConfig = new GyroConfig();
    gyroConfig->Load();

    return true;
}

static int start()
{
    gyro.start();
    // ahrs.start(); /// Not needed, called from Gyro.start()

    return DURATION_IMMEDIATELY; // Call timeout() immediately;
}

extern const char *STR_gyroMode[];
static void send_telemetry()
{
    // Attitude
    CRSF_MK_FRAME_T(crsf_sensor_attitude_t)
    crsfAttitude = {0};
    crsfAttitude.p.pitch = htobe16(ahrs.angle_rpy[GYRO_AXIS_PITCH] * 10000);
    crsfAttitude.p.roll = htobe16(ahrs.angle_rpy[GYRO_AXIS_ROLL] * 10000);
    crsfAttitude.p.yaw = htobe16(ahrs.angle_rpy[GYRO_AXIS_YAW] * 10000);

    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfAttitude, CRSF_FRAMETYPE_ATTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_attitude_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfAttitude.h);

// Don't send RAW Gyro raw data... faster telemetry
#if 0
    // Gyro
    CRSF_MK_FRAME_T(crsf_sensor_gyro_t)
    crsfGyro = {0};

    crsfGyro.p.sample_time = htobe32(millis());
    crsfGyro.p.gyro_temp = 0;

    // In Gs
    crsfGyro.p.acc_x = htobe16(ahrs.acc_rpy[GYRO_AXIS_PITCH] * 1000);
    crsfGyro.p.acc_y = htobe16(ahrs.acc_rpy[GYRO_AXIS_ROLL] * 1000);
    crsfGyro.p.acc_z = htobe16(ahrs.acc_rpy[GYRO_AXIS_YAW] * 1000);

    // From Radians/s to Deg/s
    crsfGyro.p.gyro_x = htobe16(degrees(ahrs.gyro_rpy[GYRO_AXIS_PITCH]));
    crsfGyro.p.gyro_y = htobe16(degrees(ahrs.gyro_rpy[GYRO_AXIS_ROLL]));
    crsfGyro.p.gyro_z = htobe16(degrees(ahrs.gyro_rpy[GYRO_AXIS_YAW]));

    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfGyro, CRSF_FRAMETYPE_GYRO, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gyro_t)));
    crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, &crsfGyro.h);
#endif

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

    if (!ahrs.isRunning())
        return 1000; // 1 seconds wait if not running

    if ((millis() - lastTelSent_ms) > 500)
    { // 500 ms (1/2s) cycle
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

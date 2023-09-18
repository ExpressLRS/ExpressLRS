#include "devBaro.h"

#if defined(HAS_BARO)

#include "logging.h"
#include "telemetry.h"
#include "baro_spl06.h"

#define BARO_STARTUP_INTERVAL       100

/* Shameful externs */
extern Telemetry telemetry;

/* Local statics */
static SPL06 *baro;
static eBaroReadState BaroReadState;

static bool Baro_Detect()
{
    // I2C Baros
#if defined(USE_I2C)
    if (GPIO_PIN_SCL != UNDEF_PIN && GPIO_PIN_SDA != UNDEF_PIN)
    {
        if (SPL06::detect())
        {
            DBGLN("Detected baro: SPL06");
            baro = new SPL06();
            return true;
        }
    } // I2C
#endif

    //DBGLN("No baro detected");
    return false;
}

static int Baro_Init()
{
    baro->initialize();
    if (baro->isInitialized())
    {
        // Slow down Vbat updates to save bandwidth
        Vbat_enableSlowUpdate(true);
        BaroReadState = brsReadTemp;
        return DURATION_IMMEDIATELY;
    }

    // Did not init, try again later
    return BARO_STARTUP_INTERVAL;
}

static void Baro_PublishPressure(uint32_t pressuredPa)
{
    static int32_t last_altitude_cm;
    static int16_t verticalspd_smoothed;
    int32_t altitude_cm = baro->pressureToAltitude(pressuredPa);
    int32_t altitude_diff_cm = altitude_cm - last_altitude_cm;
    last_altitude_cm = altitude_cm;

    static uint32_t last_publish_ms;
    uint32_t now = millis();
    uint32_t dT_ms = now - last_publish_ms;
    last_publish_ms = now;

    //DBGLN("%udPa %dcm", pressuredPa, altitude_cm);

    if (baro->getAltitudeHome() == BaroBase::ALTITUDE_INVALID)
    {
        baro->setAltitudeHome(altitude_cm);
        // skip sending the first reading because the VSpd will be bonkers
        return;
    }

    CRSF_MK_FRAME_T(crsf_sensor_baro_vario_t) crsfBaro = {0};

    // Item: Alt
    int32_t relative_altitude_dm = (altitude_cm - baro->getAltitudeHome()) / 10;
    if (relative_altitude_dm > (0x7FFF - 10000))
    {
        // If the altitude would be 0x8000 or higher, send it in meters with the high bit set
        crsfBaro.p.altitude = 0x8000 | (uint16_t)(relative_altitude_dm / 10);
    }
    else
    {
        // Else it is in decimeters + 10000 (-1000m max negative alt)
        crsfBaro.p.altitude = relative_altitude_dm + 10000;
    }
    crsfBaro.p.altitude = htobe16(crsfBaro.p.altitude);

    // Item: VSpd
    int16_t vspd = altitude_diff_cm * 1000 / (int32_t)dT_ms;
    verticalspd_smoothed = (verticalspd_smoothed * 3 + vspd) / 4; // Simple smoothing
    crsfBaro.p.verticalspd = htobe16(verticalspd_smoothed);
    //DBGLN("diff=%d smooth=%d dT=%u", altitude_diff_cm, verticalspd_smoothed, dT_ms);

    // if no external vario is connected output internal Vspd on CRSF_FRAMETYPE_BARO_ALTITUDE packet 
    if (!telemetry.GetCrsfBaroSensorDetected())
    {
        CRSF::SetHeaderAndCrc((uint8_t *)&crsfBaro, CRSF_FRAMETYPE_BARO_ALTITUDE, CRSF_FRAME_SIZE(sizeof(crsf_sensor_baro_vario_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
        telemetry.AppendTelemetryPackage((uint8_t *)&crsfBaro);
    }
}

static int start()
{
    if (Baro_Detect())
    {
        BaroReadState = brsUninitialized;
        return BARO_STARTUP_INTERVAL;
    }

    BaroReadState = brsNoBaro;
    return DURATION_NEVER;
}

static int timeout()
{
    if (connectionState >= MODE_STATES)
        return DURATION_NEVER;

    switch (BaroReadState)
    {
        default: // fallthrough
        case brsNoBaro:
            return DURATION_NEVER;

        case brsUninitialized:
            return Baro_Init();

        case brsWaitingTemp:
            {
                int32_t temp = baro->getTemperature();
                if (temp == BaroBase::TEMPERATURE_INVALID)
                    return DURATION_IMMEDIATELY;
                baro->startPressure();
                BaroReadState = brsWaitingPress;
                return baro->getPressureDuration();
            }

        case brsWaitingPress:
            {
                uint32_t press = baro->getPressure();
                if (press == BaroBase::PRESSURE_INVALID)
                    return DURATION_IMMEDIATELY;
                Baro_PublishPressure(press);
            }
            // fallthrough

        case brsReadTemp:
            BaroReadState = brsWaitingTemp;
            baro->startTemperature();
            return baro->getTemperatureDuration();
    }
}

device_t Baro_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
};

#endif
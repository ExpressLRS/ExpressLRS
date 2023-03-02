#include "devAnalogVbat.h"

#if defined(USE_ANALOG_VBAT)
#include <Arduino.h>
#include "CRSF.h"
#include "telemetry.h"
#include "median.h"

// Sample 5x samples over 500ms (unless SlowUpdate)
#define VBAT_SMOOTH_CNT         5
#define VBAT_SAMPLE_INTERVAL    100U

typedef uint16_t vbatAnalogStorage_t;
static MedianAvgFilter<vbatAnalogStorage_t, VBAT_SMOOTH_CNT>vbatSmooth;
static uint8_t vbatUpdateScale;

/* Shameful externs */
extern Telemetry telemetry;

/**
 * @brief: Enable SlowUpdate mode to reduce the frequency Vbat telemetry is sent
 ***/
void Vbat_enableSlowUpdate(bool enable)
{
    vbatUpdateScale = enable ? 2 : 1;
}

static int start()
{
    if (GPIO_ANALOG_VBAT == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
    vbatUpdateScale = 1;
    return VBAT_SAMPLE_INTERVAL;
}

static void reportVbat()
{
    uint32_t adc = vbatSmooth.calc_scaled();
    uint16_t vbat;
    // For negative offsets, anything between abs(OFFSET)*CNT and 0 is considered 0
    if (ANALOG_VBAT_OFFSET < 0 && adc <= (ANALOG_VBAT_OFFSET * -VBAT_SMOOTH_CNT))
        vbat = 0;
    else
        vbat = adc * 100U / (ANALOG_VBAT_SCALE * vbatSmooth.scale());

    CRSF_MK_FRAME_T(crsf_sensor_battery_t) crsfbatt = { 0 };
    // Values are MSB first (BigEndian)
    crsfbatt.p.voltage = htobe16(vbat);
    // No sensors for current, capacity, or remaining available

    CRSF::SetHeaderAndCrc((uint8_t *)&crsfbatt, CRSF_FRAMETYPE_BATTERY_SENSOR, CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
    telemetry.AppendTelemetryPackage((uint8_t *)&crsfbatt);
}

static int timeout()
{
    if (GPIO_ANALOG_VBAT == UNDEF_PIN || telemetry.GetCrsfBatterySensorDetected())
    {
        return DURATION_NEVER;
    }
    int adc = analogRead(GPIO_ANALOG_VBAT);
    adc = (adc > ANALOG_VBAT_OFFSET) ? adc - ANALOG_VBAT_OFFSET : 0;

    unsigned int idx = vbatSmooth.add(adc);
    if (idx == 0 && connectionState == connected)
        reportVbat();

    return VBAT_SAMPLE_INTERVAL * vbatUpdateScale;
}

device_t AnalogVbat_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
};

#endif /* if USE_ANALOG_VCC */
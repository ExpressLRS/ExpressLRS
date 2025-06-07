#include "devAnalogVbat.h"

#include "CRSFRouter.h"
#include "logging.h"
#include "median.h"
#include <Arduino.h>

// Sample 5x samples over 500ms (unless SlowUpdate)
#define VBAT_SMOOTH_CNT         5
#if defined(DEBUG_VBAT_ADC)
#define VBAT_SAMPLE_INTERVAL    20U // faster updates in debug mode
#else
#define VBAT_SAMPLE_INTERVAL    100U
#endif

typedef uint16_t vbatAnalogStorage_t;
static MedianAvgFilter<vbatAnalogStorage_t, VBAT_SMOOTH_CNT>vbatSmooth;
static uint8_t vbatUpdateScale;

#if defined(PLATFORM_ESP32)
#include "esp_adc_cal.h"
static esp_adc_cal_characteristics_t *vbatAdcUnitCharacterics;
#endif

/* Shameful externs */
extern bool crsfBatterySensorDetected;

/**
 * @brief: Enable SlowUpdate mode to reduce the frequency Vbat telemetry is sent
 ***/
void Vbat_enableSlowUpdate(bool enable)
{
    vbatUpdateScale = enable ? 2 : 1;
}

static bool initialize()
{
    return GPIO_ANALOG_VBAT != UNDEF_PIN;
}

static int start()
{
    vbatUpdateScale = 1;
#if defined(PLATFORM_ESP32)
    analogReadResolution(12);

    int atten = hardware_int(HARDWARE_vbat_atten);
    if (atten != -1)
    {
        // if the configured value is higher than the max item (11dB, it indicates to use cal_characterize)
        bool useCal = atten > ADC_11db;
        if (useCal)
        {
            atten -= (ADC_11db + 1);

            vbatAdcUnitCharacterics = new esp_adc_cal_characteristics_t();
            int8_t channel = digitalPinToAnalogChannel(GPIO_ANALOG_VBAT);
            adc_unit_t unit = (channel > (SOC_ADC_MAX_CHANNEL_NUM - 1)) ? ADC_UNIT_2 : ADC_UNIT_1;
            esp_adc_cal_characterize(unit, (adc_atten_t)atten, ADC_WIDTH_BIT_12, 3300, vbatAdcUnitCharacterics);
        }
        analogSetPinAttenuation(GPIO_ANALOG_VBAT, (adc_attenuation_t)atten);
    }
#endif

    return VBAT_SAMPLE_INTERVAL;
}

static void reportVbat()
{
    uint32_t adc = vbatSmooth.calc();
#if defined(PLATFORM_ESP32) && !defined(DEBUG_VBAT_ADC)
    if (vbatAdcUnitCharacterics)
        adc = esp_adc_cal_raw_to_voltage(adc, vbatAdcUnitCharacterics);
#endif

    int32_t vbat;
    // For negative offsets, anything between abs(OFFSET) and 0 is considered 0
    if (ANALOG_VBAT_OFFSET < 0 && adc <= -ANALOG_VBAT_OFFSET)
        vbat = 0;
    else
        vbat = ((int32_t)adc - ANALOG_VBAT_OFFSET) * 100 / ANALOG_VBAT_SCALE;

    CRSF_MK_FRAME_T(crsf_sensor_battery_t) crsfbatt = { 0 };
    // Values are MSB first (BigEndian)
    crsfbatt.p.voltage = htobe16((uint16_t)vbat);
    // No sensors for current, capacity, or remaining available

    crsfRouter.SetHeaderAndCrc((crsf_header_t *)&crsfbatt, CRSF_FRAMETYPE_BATTERY_SENSOR, CRSF_FRAME_SIZE(sizeof(crsf_sensor_battery_t)), CRSF_ADDRESS_RADIO_TRANSMITTER);
    crsfRouter.deliverMessage(nullptr, &crsfbatt.h);
}

static int timeout()
{
    if (crsfBatterySensorDetected)
    {
        return DURATION_NEVER;
    }

    uint32_t adc = analogRead(GPIO_ANALOG_VBAT);
#if defined(PLATFORM_ESP32) && defined(DEBUG_VBAT_ADC)
    // When doing DEBUG_VBAT_ADC, every value is adjusted (for logging)
    // in normal mode only the final value is adjusted to save CPU cycles
    if (vbatAdcUnitCharacterics)
        adc = esp_adc_cal_raw_to_voltage(adc, vbatAdcUnitCharacterics);
    DBGLN("$ADC,%u", adc);
#endif

    unsigned int idx = vbatSmooth.add(adc);
    if (idx == 0 && connectionState == connected)
        reportVbat();

    return VBAT_SAMPLE_INTERVAL * vbatUpdateScale;
}

device_t AnalogVbat_device = {
    .initialize = initialize,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
    .subscribe = EVENT_NONE
};

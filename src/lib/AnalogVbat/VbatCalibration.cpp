#include "VbatCalibration.h"

#include <Arduino.h>

#if defined(PLATFORM_ESP32)
#include "esp_adc_cal.h"
#endif

#if defined(PLATFORM_ESP8266)
static constexpr uint8_t VOLTAGE_SOURCE_COUNT = 1;
static constexpr uint16_t ADC_MAX_VALUE = 1023;
static constexpr uint16_t ADC_SATURATION_MARGIN = 6;
#else
static constexpr uint8_t VOLTAGE_SOURCE_COUNT =4;
static constexpr uint16_t ADC_MAX_VALUE = 4095;
static constexpr uint16_t ADC_SATURATION_MARGIN = 24;
#endif

#if defined(PLATFORM_ESP32)
static constexpr int CALIBRATED_ATTENUATION_START = 4;
#endif

typedef struct {
    const char *id;
    const char *label;
    nameType hardwarePin;
    nameType offset;
    nameType scale;
    nameType atten;
    nameType noReading;
    nameType calMin;
    nameType calMax;
} voltageSourceCalibration_t;

static const voltageSourceCalibration_t voltageSources[VOLTAGE_SOURCE_COUNT] = {
    {"vbat", "VBat", HARDWARE_vbat, HARDWARE_vbat_offset, HARDWARE_vbat_scale, HARDWARE_vbat_atten, HARDWARE_vbat_noreading, HARDWARE_vbat_cal_min, HARDWARE_vbat_cal_max},
#if defined(PLATFORM_ESP32)
    {"vsrc1", "VSrc1", HARDWARE_vsrc1, HARDWARE_vsrc1_offset, HARDWARE_vsrc1_scale, HARDWARE_vsrc1_atten, HARDWARE_vsrc1_noreading, HARDWARE_vsrc1_cal_min, HARDWARE_vsrc1_cal_max},
    {"vsrc2", "VSrc2", HARDWARE_vsrc2, HARDWARE_vsrc2_offset, HARDWARE_vsrc2_scale, HARDWARE_vsrc2_atten, HARDWARE_vsrc2_noreading, HARDWARE_vsrc2_cal_min, HARDWARE_vsrc2_cal_max},
    {"vsrc3", "VSrc3", HARDWARE_vsrc3, HARDWARE_vsrc3_offset, HARDWARE_vsrc3_scale, HARDWARE_vsrc3_atten, HARDWARE_vsrc3_noreading, HARDWARE_vsrc3_cal_min, HARDWARE_vsrc3_cal_max}
#endif
};

static bool sourceIsDefined(const uint8_t sourceIdx)
{
    return hardware_pin(voltageSources[sourceIdx].hardwarePin) != UNDEF_PIN;
}

#if defined(PLATFORM_ESP32)
static int getSamplingAttenuation(const int atten)
{
    if (atten >= CALIBRATED_ATTENUATION_START)
        return atten - CALIBRATED_ATTENUATION_START;
    if (atten == -1)
        return ADC_11db;
    return atten;
}
#endif

static uint32_t convertRawToAdcDomain(const uint8_t sourceIdx, const int atten, const uint32_t raw)
{
#if defined(PLATFORM_ESP32)
    if (atten > ADC_11db)
    {
        esp_adc_cal_characteristics_t characteristics;
        const int sourcePin = hardware_pin(voltageSources[sourceIdx].hardwarePin);
        const int8_t channel = digitalPinToAnalogChannel(sourcePin);
        const adc_unit_t unit = (channel > (SOC_ADC_MAX_CHANNEL_NUM - 1)) ? ADC_UNIT_2 : ADC_UNIT_1;
        esp_adc_cal_characterize(unit, (adc_atten_t)getSamplingAttenuation(atten), ADC_WIDTH_BIT_12, 3300, &characteristics);
        return esp_adc_cal_raw_to_voltage(raw, &characteristics);
    }
#endif
    return raw;
}

static void sortSamples(uint16_t *values, const uint8_t count)
{
    for (uint8_t i = 1; i < count; ++i)
    {
        const uint16_t value = values[i];
        int pos = i - 1;
        while (pos >= 0 && values[pos] > value)
        {
            values[pos + 1] = values[pos];
            --pos;
        }
        values[pos + 1] = value;
    }
}

static void summarizeSamples(uint16_t *rawValues, uint16_t *adcValues, const uint8_t count, voltage_source_sample_t *sample)
{
    sortSamples(rawValues, count);
    sortSamples(adcValues, count);
    sample->rawMax = rawValues[count-1];
    if ((count & 1) == 0)
    {
        sample->rawMedian = (rawValues[(count / 2) - 1] + rawValues[count / 2]) / 2;
        sample->adcMedian = (adcValues[(count / 2) - 1] + adcValues[count / 2]) / 2;
    }
    else
    {
        sample->rawMedian = rawValues[count / 2];
        sample->adcMedian = adcValues[count / 2];
    }
}

uint8_t VbatCalibration_getSourceCount()
{
    return VOLTAGE_SOURCE_COUNT;
}

bool VbatCalibration_findSource(const char *sourceId, uint8_t *sourceIdx)
{
    if (!sourceId)
        return false;

    for (uint8_t idx = 0; idx < VOLTAGE_SOURCE_COUNT; ++idx)
    {
        if (strcmp(sourceId, voltageSources[idx].id) == 0)
        {
            if (sourceIdx)
                *sourceIdx = idx;
            return true;
        }
    }

    return false;
}

bool VbatCalibration_isSourceDefined(const uint8_t sourceIdx)
{
    return sourceIdx < VOLTAGE_SOURCE_COUNT && sourceIsDefined(sourceIdx);
}

void VbatCalibration_getSourceConfig(const uint8_t sourceIdx, voltage_source_config_t *config)
{
    if (!config || sourceIdx >= VOLTAGE_SOURCE_COUNT)
        return;

    config->id = voltageSources[sourceIdx].id;
    config->label = voltageSources[sourceIdx].label;
    config->pin = hardware_pin(voltageSources[sourceIdx].hardwarePin);
    config->offset = hardware_int(voltageSources[sourceIdx].offset);
    config->scale = hardware_int(voltageSources[sourceIdx].scale);
    config->atten = hardware_int(voltageSources[sourceIdx].atten);
    config->noReading = hardware_int(voltageSources[sourceIdx].noReading);
    config->calMin = hardware_int(voltageSources[sourceIdx].calMin);
    config->calMax = hardware_int(voltageSources[sourceIdx].calMax);
}

bool VbatCalibration_sampleSource(const uint8_t sourceIdx, int atten, uint8_t samples, voltage_source_sample_t *sample)
{
    if (!sample || sourceIdx >= VOLTAGE_SOURCE_COUNT || !sourceIsDefined(sourceIdx))
        return false;

    samples = constrain(samples, 24, 64);
    memset(sample, 0, sizeof(*sample));

#if defined(PLATFORM_ESP32)
    analogReadResolution(12);
    analogSetPinAttenuation(hardware_pin(voltageSources[sourceIdx].hardwarePin), (adc_attenuation_t)getSamplingAttenuation(atten));
#endif

    uint16_t rawValues[samples] {};
    uint16_t adcValues[samples] {};
    for (uint8_t i = 0; i < samples; ++i)
    {
        const uint16_t raw = analogRead(hardware_pin(voltageSources[sourceIdx].hardwarePin));
        rawValues[i] = raw;
        adcValues[i] = convertRawToAdcDomain(sourceIdx, atten, raw);
    }

    summarizeSamples(rawValues, adcValues, samples, sample);
    sample->saturated = sample->rawMax >= (ADC_MAX_VALUE - ADC_SATURATION_MARGIN);
    const int noReadingThreshold = hardware_int(voltageSources[sourceIdx].noReading);
    sample->hasReading = (noReadingThreshold < 0) || (sample->rawMedian > (uint16_t)noReadingThreshold);
    return true;
}

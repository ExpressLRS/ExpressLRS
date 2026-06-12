#pragma once

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)

#include "common.h"

typedef struct {
    const char *id;
    const char *label;
    int pin;
    int offset;
    int scale;
    int atten;
    int noReading;
    int calMin;
    int calMax;
} voltage_source_config_t;

typedef struct {
    uint16_t rawMax;
    uint16_t rawMedian;
    uint16_t adcMedian;
    bool saturated;
    bool hasReading;
} voltage_source_sample_t;

uint8_t VbatCalibration_getSourceCount();
bool VbatCalibration_findSource(const char *sourceId, uint8_t *sourceIdx);
bool VbatCalibration_isSourceDefined(uint8_t sourceIdx);
void VbatCalibration_getSourceConfig(uint8_t sourceIdx, voltage_source_config_t *config);
bool VbatCalibration_sampleSource(uint8_t sourceIdx, int atten, uint8_t samples, voltage_source_sample_t *sample);

#endif

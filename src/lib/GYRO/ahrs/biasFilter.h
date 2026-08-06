#pragma once
#include "gyro_types.h"

typedef struct {
    float sampleRate; // Hz
    float stationaryThreshold; // rad per second
    float stationaryPeriod; // seconds
} BiasSettings;

class BiasFilter
{
protected:
    BiasSettings settings;
    float filterCoefficient;
    unsigned int timeout;
    unsigned int timer;
    VectorFloat offset;

public:
    void Initialise(const float sampleRate);
    void Update(VectorFloat gyroscope);
    VectorFloat getOffsets();
};

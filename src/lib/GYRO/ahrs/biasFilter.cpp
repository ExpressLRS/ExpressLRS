/**
 * Adaptation From Fusion Bias Filter
 * by Seb Madgwick
 * @brief Run-time estimation and compensation of gyroscope offset.
 */

#include <Arduino.h>
#include "biasFilter.h"
#include "logging.h"

/**
 * @brief High-pass filter cutoff frequency in Hz.
 */
#define CUTOFF_FREQUENCY (0.02f)

//------------------------------------------------------------------------------
// Variables

static BiasSettings gyroBiasDefaultSettings = {
    .sampleRate = 100.0f,
    .stationaryThreshold = 3.0f,  // 3-Degress 
    .stationaryPeriod = 3.0f, // 3 cycles of stationary
};


//------------------------------------------------------------------------------
// Functions

/**
 * @brief Initialises the bias structure.
 * @param bias Bias structure.
 */
void BiasFilter::Initialise(const float sampleRate) {
    gyroBiasDefaultSettings.sampleRate = sampleRate;
    settings = gyroBiasDefaultSettings;
    filterCoefficient = 2.0f * (float) M_PI  * CUTOFF_FREQUENCY * (1.0f / settings.sampleRate);
    timeout = (unsigned int) (settings.stationaryPeriod * settings.sampleRate);

    timer = 0;
    offset.x = 0;
    offset.y = 0;
    offset.z = 0;

    #if defined(DEBUG_LOG)
    DBGLN("Gyro Bias SampleRate=%f Bias Timeout = %d",sampleRate, timeout);
    char a[15]; sprintf(a, "%1.6f", filterCoefficient);
    char b[15]; sprintf(b, "%1.6f", settings.stationaryThreshold);
    DBGLN("Bias filterCoeficient=%s, Threshold=%s ",a,b);
    #endif
}

/**
 * @brief Updates the bias algorithm and returns the offset-corrected
 * gyroscope. This function must be called for every gyroscope sample at the
 * configured sample rate.
 * @param gyroscope Gyroscope in radians per second.
 * @return Offset-corrected gyroscope in radians per second.
 */
void BiasFilter::Update(VectorFloat gyroscope) {
    // Apply gyroscope offset
    gyroscope.x -= offset.x;
    gyroscope.y -= offset.y;
    gyroscope.z -= offset.z;

    // Reset timer if gyroscope not stationary
    if ((fabsf(gyroscope.x) > settings.stationaryThreshold) ||
        (fabsf(gyroscope.y) > settings.stationaryThreshold) ||
        (fabsf(gyroscope.z) > settings.stationaryThreshold)) {
        timer = 0;
        return; // Not Stationary
    }

    // Increment timer while gyroscope stationary
    if (timer < timeout) {
        timer++;
        return;
    }

    // Update high-pass filter while timer has elapsed
    offset.x += gyroscope.x * filterCoefficient;
    offset.y += gyroscope.y * filterCoefficient;
    offset.z += gyroscope.z * filterCoefficient;
}

VectorFloat BiasFilter::getOffsets() {
    return offset;
}

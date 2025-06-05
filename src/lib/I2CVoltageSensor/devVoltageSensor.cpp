#include "devVoltageSensor.h"

#include <cstdint>
#include <utility>

#include "common.h"
#include "crsf_protocol.h"
#include "device.h"
#include "logging.h"
#include "telemetry.h"

#include "ads1115.h"

// Update interval for the ADC values
#define VOLTAGE_SENSE_INTERVAL 200
// Minimal update interval
#define VOLTAGE_SENSE_MIN_INTERVAL 50

// For no particular reason delay the first execution a bit
#define VOLTAGE_SENSE_STARTUP_INTERVAL VOLTAGE_SENSE_INTERVAL * 4

// Arbitray amount of maximum supported i2c ADCs, 4x4 should be enoug for everyone!
#define MAX_ADCS 4

extern Telemetry telemetry;

extern bool i2c_enabled;

static I2CVoltageSensorADCBase *adcs[4] = {nullptr};
static size_t num_adcs = 0;

static bool detect_adc()
{
#if defined(USE_I2C)
    if (i2c_enabled)
    {
        DBGLN("Detecting adcs");
        // Detect which ADCs are available

        for (auto address : {ADS1115Address::GND, ADS1115Address::SCL, ADS1115Address::SDA, ADS1115Address::VDD})
        {

            if (ADS1115::detect(address))
            {
                DBGLN("Detected ADS1115");
                if (num_adcs >= MAX_ADCS)
                {
                    DBGLN("Found another ADS1115 but I'm running out of slots for them :(");
                    break;
                }

                auto adc = new ADS1115(address);
                adc->configure();
                adcs[num_adcs++] = adc;
            }
        }

        if (num_adcs > 0)
            return true;
    }
#endif
    return false;
}

static int start()
{
    if (detect_adc())
    {
        return VOLTAGE_SENSE_STARTUP_INTERVAL;
    }

    return DURATION_NEVER;
}

static int timeout()
{
    if (num_adcs == 0)
        return DURATION_NEVER;

    if (connectionState >= MODE_STATES)
        return DURATION_NEVER;

    auto next_delay = VOLTAGE_SENSE_INTERVAL;
    for (uint8_t i = 0; i < num_adcs; i++)
    {
        // DBGLN("Updating voltage sensor values of adc %l", i);
        CRSF_MK_FRAME_T(crsf_sensor_cells_t)
        cells = {.p = {.source_id = i, .cell = {0}}};
        constexpr uint8_t max_cells = sizeof(cells.p.cell) / sizeof(cells.p.cell[0]);

        auto adc = adcs[i];
        // Figure out when we should call this ADC again. We use the
        // minimum value across all of them to delay the execution fo
        // this function.
        auto requested_delay = adc->update();
        next_delay = std::min(next_delay, requested_delay);
        if (!adc->isReady())
            continue;

        const size_t n = std::min(adc->getNumChannels(), max_cells);
        for (auto cell = 0; cell < n; cell++)
        {
            // FIXME: scaling
            auto value = adc->getReading(cell);
            // DBGLN("value: %x", value);
            cells.p.cell[cell] = value;
        }

        // FIXME: add detection for external cells sensor?
        if (!/*telemetry.GetCrsfCellsSensorDetected()*/ false)
        {
            CRSF::SetHeaderAndCrc((uint8_t *)&cells, CRSF_FRAMETYPE_CELLS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_cells_t)), CRSF_ADDRESS_RADIO_TRANSMITTER);
            telemetry.AppendTelemetryPackage((uint8_t *)&cells);
        }
    }

    return std::max(VOLTAGE_SENSE_MIN_INTERVAL, std::min(next_delay, VOLTAGE_SENSE_INTERVAL));
}

device_t I2CVoltageSensor = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout,
};

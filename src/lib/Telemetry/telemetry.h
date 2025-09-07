#pragma once

#include "CRSFConnector.h"

typedef enum {
    TELEMETRY_IDLE = 0,
    RECEIVING_LENGTH,
    RECEIVING_DATA
} telemetry_state_s;

/**
 * Represents a telemetry management system used for handling communication, processing
 * data streams over UART, and managing the detection of external telemetry sensors.
 */
class Telemetry
{
public:
    Telemetry() = default;

    /**
     * Handles incoming UART data for telemetry by processing received bytes and managing the current state.
     * Based on the telemetry state, it validates data and processes packets to extract telemetry information.
     *
     * @param origin Pointer to the CRSFConnector object that represents the data source.
     * @param data The received byte from the UART.
     * @return True if the data is successfully processed or forwarded for handling, false if the data is ignored
     *         or invalid.
     */
    bool RXhandleUARTin(CRSFConnector *origin, uint8_t data);

    /**
     * Marks the CRSF battery sensor as detected by setting the corresponding state
     * variable. This function is used to indicate the presence of an external
     * battery sensor for telemetry purposes.
     */
    void SetCrsfBatterySensorDetected() { crsfBatterySensorDetected = true; }

    /**
     * Retrieves the current detection status of the CRSF battery sensor.
     * Indicates whether an external battery sensor for telemetry purposes
     * has been detected. If an external battery sensor is present, then
     * any inbuilt battery voltage sensing is effectively disabled.
     *
     * @return True if the CRSF battery sensor is detected, false otherwise.
     */
    bool GetCrsfBatterySensorDetected() const { return crsfBatterySensorDetected; }

    /**
     * Marks the CRSF barometric sensor as detected by setting the corresponding state
     * variable. This function is used to indicate the presence of an external
     * barometric sensor for telemetry purposes.
     */
    void SetCrsfBaroSensorDetected() { crsfBaroSensorDetected = true; }

    /**
     * Retrieves the detection status of the CRSF barometric sensor.
     * Indicates whether an external barometric sensor for telemetry
     * purposes has been detected. If an external barometric sensor is
     * present, then any inbuilt barometric sensor is effectively disabled.
     *
     * @return True if the CRSF barometric sensor is detected, false otherwise.
     */
    bool GetCrsfBaroSensorDetected() const { return crsfBaroSensorDetected; }

    // unit testing
    void Reset()
    {
        currentTelemetryByte = 0;
        telemetry_state = TELEMETRY_IDLE;
    }

private:
    void CheckCrsfBatterySensorDetected();
    void CheckCrsfBaroSensorDetected();

    uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN] {};
    telemetry_state_s telemetry_state = TELEMETRY_IDLE;
    uint8_t currentTelemetryByte = 0;

    bool crsfBatterySensorDetected = false;
    bool crsfBaroSensorDetected = false;
};

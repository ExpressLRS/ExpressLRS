#pragma once

#include "device.h"

/**
 * @brief process backpack PTR (pan/tilt/roll) MSP packet from goggles/head tracker.
 *
 * @param now current time in millis
 * @param packet the msp packet containing the PTR MSP packet
 */
void processPanTiltRollPacket(uint32_t now, mspPacket_t *packet);

/**
 * @brief perform check to see if a backpack firmware update has been requested.
 *
 * If a backpack update has been requested then all devices are stopped and the UARTs
 * are configured for passthrough flashing.
 */
void checkBackpackUpdate();

/**
 * @brief send CRSF telemetry packet to the backpack.
 *
 * The CRSF packet is wrapped in an MSP packet of type `MSP_ELRS_BACKPACK_CRSF_TLM`
 * and forwarded to the backpack.
 *
 * @param data the CRSF telemetry packet to send.
 */
void sendCRSFTelemetryToBackpack(uint8_t *data);

/**
 * @brief send MAVLink telemetry packet (inside a CRSF packet) to the backpack.
 *
 * @param data the MAVLink telemetry packet to send.
 */
void sendMAVLinkTelemetryToBackpack(uint8_t *data);

extern device_t Backpack_device;

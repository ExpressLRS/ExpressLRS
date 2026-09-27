// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

// Handset MAVLink transport for TX builds only.
//
// The handset callback merely queues inbound frames; subscription, heartbeat, target
// and write checks all run later on the TX main loop, so no bridge state is ever
// shared between the two contexts. Link loss or leaving MAVLink mode clears everything.

/**
 * Offers a raw CRSF frame from the handset to the bridge queue.
 *
 * Called from the handset callback. Only frame type 0xAA is claimed; the frame is
 * copied into a bounded FIFO when type and length are plausible, otherwise it is
 * rejected without being altered. No aircraft traffic is generated here.
 *
 * @param frame Pointer to the whole CRSF frame, beginning with the sync byte.
 * @return True when the frame is a handset MAVLink envelope and must not be routed.
 */
bool mavLuaEnqueue(const uint8_t *frame);

/**
 * Runs one step of TX-side bridge processing.
 *
 * Called once per TX main loop. Forwards at most one validated handset request into the
 * existing MAVLink uplink, and queues a paced SYS_STATUS request when readiness telemetry
 * is missing. Resets the bridge and drains the queue when not connected in MAVLink mode.
 */
void mavLuaPoll();

/**
 * Reports a parsed downlink message to the readiness tracker.
 *
 * Only HEARTBEAT and SYS_STATUS need to be observed; other message IDs are ignored.
 * Used to discover the autopilot and to suppress redundant SYS_STATUS requests.
 *
 * @param id MAVLink message ID.
 * @param sys Source system ID.
 * @param comp Source component ID.
 * @param payload Pointer to the decoded message payload.
 */
void mavLuaObserve(uint32_t id, uint8_t sys, uint8_t comp, const uint8_t *payload);

/**
 * Offers a parsed downlink message to the parameter bridge for handset forwarding.
 *
 * The payload and full packet have already been validated by the existing MAVLink
 * parser. The bridge forwards the packet to the handset only when the message answers an
 * outstanding request from the currently targeted autopilot; the packet may be split into
 * bounded chunks. Nothing is written to the aircraft here.
 *
 * @param id MAVLink message ID.
 * @param sys Source system ID.
 * @param comp Source component ID.
 * @param payload Pointer to the decoded message payload.
 * @param packet Pointer to the complete encoded MAVLink packet.
 * @param length Length of the encoded packet in bytes.
 */
void mavLuaDownlink(uint32_t id, uint8_t sys, uint8_t comp, const uint8_t *payload,
                    const uint8_t *packet, uint16_t length);


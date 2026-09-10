#pragma once

/**
 * Replacement for the c_library_v2 message entry lookup.
 *
 * The default mavlink_get_msg_entry() keeps the CRC_EXTRA / length table as a
 * plain const inside a static inline helper, which on ESP8266 puts ~2.6KB in
 * DRAM (and one copy per translation unit that includes the helpers). Our
 * version keeps a single copy in flash (PROGMEM) and reads it with pgm
 * accessors.
 *
 * Include this header before "common/mavlink.h" in every file that includes
 * the MAVLink headers, otherwise that file silently gets the default table.
 */

#include <stdint.h>
#include "mavlink_types.h"

#define MAVLINK_GET_MSG_ENTRY

const mavlink_msg_entry_t *mavlink_get_msg_entry(uint32_t msgid);

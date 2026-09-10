#include <Arduino.h>

#include "helpers.h"
#include "mavlink_msg_entry.h"

#define MAVLINK_COMM_NUM_BUFFERS 1
#include "common/mavlink.h"

// The table the dialect header hands us, sorted by msgid, but in flash.
static const mavlink_msg_entry_t mavlink_message_crcs[] PROGMEM = MAVLINK_MESSAGE_CRCS;

/**
 * Same bisection as the stock helper, but the probes read msgid (a 32-bit,
 * 4-byte aligned field, so a single word load from flash) and the matching
 * entry is copied out for the caller.
 *
 * The parser calls this once per received frame from the main loop, so a
 * single static scratch entry is enough: nothing on either target parses
 * MAVLink from interrupt context.
 */
const mavlink_msg_entry_t *mavlink_get_msg_entry(uint32_t msgid)
{
    static mavlink_msg_entry_t entry;

    uint32_t low = 0;
    uint32_t high = ARRAY_SIZE(mavlink_message_crcs) - 1;
    while (low < high)
    {
        const uint32_t mid = (low + 1 + high) / 2;
        const uint32_t midId = pgm_read_dword(&mavlink_message_crcs[mid].msgid);
        if (msgid < midId)
        {
            high = mid - 1;
            continue;
        }
        if (msgid > midId)
        {
            low = mid;
            continue;
        }
        low = mid;
        break;
    }
    if (pgm_read_dword(&mavlink_message_crcs[low].msgid) != msgid)
    {
        // msgid is not in the table
        return NULL;
    }
    memcpy_P(&entry, &mavlink_message_crcs[low], sizeof(entry));
    return &entry;
}

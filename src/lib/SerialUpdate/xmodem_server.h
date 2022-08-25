/**
 * Implementation of the receiver side of the XModem data transfer protocol
 * This implementation has been done as an asynchronous system, so there
 * are no blocking read calls.
 * It does not allocate any dynamic memory, using only ~160B of memory
 * while the transfer is in progress
 */
#ifndef XMODEM_SERVER_H
#define XMODEM_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * Original XModem only supports 128B transfers, but Xmodem-1k supports up to
 * 1k transfers. This is automatically detected by the presense of the STX
 * instead of the SOH character as the transfer start.
 * This must be defined to either 128 or 1024. Unless memory usage is a concern,
 * this should be left at 1024
 */
#ifndef XMODEM_MAX_PACKET_SIZE
#define XMODEM_MAX_PACKET_SIZE 1024
#endif

/**
 * The different states that the internal xmodem state machine may be in
 */
typedef enum {
	XMODEM_STATE_START,
	XMODEM_STATE_SOH,
	XMODEM_STATE_BLOCK_NUM,
	XMODEM_STATE_BLOCK_NEG,
	XMODEM_STATE_DATA,
	XMODEM_STATE_CRC0,
	XMODEM_STATE_CRC1,
	XMODEM_STATE_PROCESS_PACKET,
	XMODEM_STATE_SUCCESSFUL,
	XMODEM_STATE_FAILURE,

	XMODEM_STATE_COUNT,
} xmodem_server_state;

struct xmodem_server;

/**
 * Callback function to transmit a byte to the xmodem client
 */
typedef void (*xmodem_tx_byte)(struct xmodem_server *xdm, uint8_t byte, void *cb_data);

/**
 * This contains the state for the xmodem server.
 * None of its contents should be accessed directly, this structure
 * should be considered opaque
 */
struct xmodem_server {
	xmodem_server_state state; // What state are we in?
	uint8_t packet_data[XMODEM_MAX_PACKET_SIZE]; // Incoming packet data
	int packet_pos; // Where are we up to in this packet
	uint16_t crc; // Whatis the expected CRC of the incoming packet
	uint16_t packet_size; // Are we receiving 128B or 1K packets?
	bool repeating; // Are we receiving a packet that we've already processed?
	int64_t last_event_time; // When did we last do something interesting?
	uint32_t block_num; // What block are we up to?
	uint32_t error_count; // How many errors have we seen?
	xmodem_tx_byte tx_byte;
	void *cb_data;
};

/**
 * Initialise the internal xmodem server state
 * @param xdm Xmodem server state area to initialise
 * @param tx_byte callback to be called for ACK/NACK bytes
 * @param cb_data user-supplied pointer to be supplied to the tx_byte function
 * @return < 0 on failure, >= 0 on success
 */
int xmodem_server_init(struct xmodem_server *xdm, xmodem_tx_byte tx_byte, void *cb_data);

/**
 * Send a single byte to the xmodem state machine
 * @returns true if a packet is now available for processing, false if more data is needed
 */
bool xmodem_server_rx_byte(struct xmodem_server *xdm, uint8_t byte);

/**
 * Determine the current state of the xmodem transfer
 */
xmodem_server_state xmodem_server_get_state(const struct xmodem_server *xdm);

/**
 * Returns a human readable version of the current state
 */
const char *xmodem_server_state_name(const struct xmodem_server *xdm);

/**
 * Utility function for extending the XModem 16-bit CRC calculate by
 * one byte.
 * Note: This function should not normally be need to be called explicitly,
 * it is provided to make writing test cases easier
 */
uint16_t xmodem_server_crc(uint16_t crc, uint8_t byte);

/**
 * Process the internal state and determine if there is a full packet ready
 * This function should be called periodically to correctly process timeouts
 * @param xdm xmodem_server state
 * @param packet Area to store the next decoded packet. Must be at least XMODEM_MAX_PACKET_SIZE long. xdm->packet_size bytes will be copied in here
 * @param block_num Area to store the 0-based index of the extracted block
 * @param ms_time Current time in milliseconds (used to determine timeouts)
 * @return Number of bytes of data copied into 'packet' (either 128, or 1024), or 0 if no new packet is available
 */
int xmodem_server_process(struct xmodem_server *xdm, uint8_t *packet, uint32_t *block_num, int64_t ms_time);

/**
 * Determine if the transfer is complete (success or failure)
 * @return true if the transfer has been finished, false otherwise
 */
bool xmodem_server_is_done(const struct xmodem_server *xdm);

#ifdef __cplusplus
}
#endif

#endif

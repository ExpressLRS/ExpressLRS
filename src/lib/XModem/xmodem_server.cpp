#include <string.h>

#include "xmodem_server.h"

/* XMODEM protocol constants */
#define XMODEM_SOH 0x01
#define XMODEM_STX 0x02
#define XMODEM_EOT 0x04
#define XMODEM_ACK 0x06
#define XMODEM_NACK 0x15
#define XMODEM_CAN 0x18

// How many milliseconds do we have to wait for a packet to arrive before
// we send a NAK & restart the transfer
#define XMODEM_PACKET_TIMEOUT 1000

// How many errors during the transfer before we just fail
// TODO: Could this be configurable
#define XMODEM_MAX_ERRORS 10

static const char *state_name(xmodem_server_state state) {
	#define XDMSTAT(a) case XMODEM_STATE_ ##a: return #a
	switch(state) {
		XDMSTAT(START);
		XDMSTAT(SOH);
		XDMSTAT(BLOCK_NUM);
		XDMSTAT(BLOCK_NEG);
		XDMSTAT(DATA);
		XDMSTAT(CRC0);
		XDMSTAT(CRC1);
		XDMSTAT(PROCESS_PACKET);
		XDMSTAT(SUCCESSFUL);
		XDMSTAT(FAILURE);
		default: return "UNKNOWN";
	}
	#undef XDMSTAT
}

uint16_t xmodem_server_crc(uint16_t crc, uint8_t byte)
{
	crc = crc ^ ((uint16_t)byte) << 8;
	for (int i = 8; i > 0; i--) {
		if (crc & 0x8000)
			crc = crc << 1 ^ 0x1021;
		else
			crc = crc << 1;
	}
	return crc;
}

bool xmodem_server_rx_byte(struct xmodem_server *xdm, uint8_t byte) {
	switch (xdm->state) {
	case XMODEM_STATE_START:
	case XMODEM_STATE_SOH:
		if (byte == XMODEM_SOH) {
			xdm->state = XMODEM_STATE_BLOCK_NUM;
			xdm->packet_size = 128;
#if XMODEM_MAX_PACKET_SIZE == 1024
		} else if (byte == XMODEM_STX) {
			xdm->state = XMODEM_STATE_BLOCK_NUM;
			xdm->packet_size = 1024;
#endif
		} else if (byte == XMODEM_EOT) {
			xdm->state = XMODEM_STATE_SUCCESSFUL;
			xdm->tx_byte(xdm, XMODEM_ACK, xdm->cb_data);
		}
		break;
	case XMODEM_STATE_BLOCK_NUM:
		if (byte == ((xdm->block_num + 1) & 0xff)) {
			xdm->state = XMODEM_STATE_BLOCK_NEG;
			xdm->repeating = false;
		} else if (byte == (xdm->block_num & 0xff)) {
			xdm->state = XMODEM_STATE_BLOCK_NEG;
			xdm->repeating = true;
		} else if (byte == XMODEM_SOH || byte == XMODEM_STX) {
			xdm->state = XMODEM_STATE_BLOCK_NUM;
		} else {
			xdm->state = XMODEM_STATE_SOH;
		}
		break;

	case XMODEM_STATE_BLOCK_NEG: {
		uint8_t neg_block = ~(xdm->block_num + 1) & 0xff;
		if (xdm->repeating)
			neg_block = (~xdm->block_num) & 0xff;
		if (byte == neg_block) {
			xdm->packet_pos = 0;
			xdm->state = XMODEM_STATE_DATA;
		} else if (byte == XMODEM_SOH || byte == XMODEM_STX) {
			xdm->state = XMODEM_STATE_BLOCK_NUM;
		} else {
			xdm->state =XMODEM_STATE_SOH;
		}
		break;
	}
	case XMODEM_STATE_DATA:
		xdm->packet_data[xdm->packet_pos++] = byte;
		if (xdm->packet_pos >= xdm->packet_size)
			xdm->state = XMODEM_STATE_CRC0;
		break;

	case XMODEM_STATE_CRC0:
		xdm->crc = ((uint16_t)byte) << 8;
		xdm->state = XMODEM_STATE_CRC1;
		break;

	case XMODEM_STATE_CRC1: {
		uint16_t crc = 0;
		xdm->crc |= byte;
		for (int i = 0; i < xdm->packet_size; i++)
			crc = xmodem_server_crc(crc, xdm->packet_data[i]);
		if (crc != xdm->crc) {
			xdm->error_count++;
			xdm->state = XMODEM_STATE_SOH;
			xdm->tx_byte(xdm, XMODEM_NACK, xdm->cb_data);
		} else if (xdm->repeating) {
			//xdm->tx_byte(xdm, XMODEM_ACK, xdm->cb_data);
			xdm->state = XMODEM_STATE_SOH;
		} else {
			xdm->state = XMODEM_STATE_PROCESS_PACKET;
		}
		break;
	}

	default:
		break;
	}

	return (xdm->state == XMODEM_STATE_PROCESS_PACKET);
}

const char *xmodem_server_state_name(const struct xmodem_server *xdm)
{
	return state_name(xdm->state);
}

int xmodem_server_init(struct xmodem_server *xdm, xmodem_tx_byte tx_byte, void *cb_data) {
	if (!tx_byte)
		return -1;
	memset(xdm, 0, sizeof(*xdm));
	xdm->tx_byte = tx_byte;
	xdm->cb_data = cb_data;

	xdm->tx_byte(xdm, 'C', xdm->cb_data);

	return 0;
}

xmodem_server_state xmodem_server_get_state(const struct xmodem_server *xdm) {
	return xdm->state;
}

bool xmodem_server_is_done(const struct xmodem_server *xdm) {
	return xdm->state == XMODEM_STATE_SUCCESSFUL || xdm->state == XMODEM_STATE_FAILURE;
}

int xmodem_server_process(struct xmodem_server *xdm, uint8_t *packet, uint32_t *block_num, int64_t ms_time) {
	if (xmodem_server_is_done(xdm))
		return 0;
	// Avoid confusion with 0 default value
	if (ms_time == 0)
		ms_time = 1;
	// Initialise our timer
	if (xdm->last_event_time == 0)
		xdm->last_event_time = ms_time;
	if (xdm->state == XMODEM_STATE_START && ms_time - xdm->last_event_time > 500) {
		xdm->tx_byte(xdm, 'C', xdm->cb_data);
		xdm->last_event_time = ms_time;
	}
	if (ms_time - xdm->last_event_time > XMODEM_PACKET_TIMEOUT) {
		xdm->error_count++;
		xdm->state = XMODEM_STATE_SOH;
		xdm->tx_byte(xdm, XMODEM_NACK, xdm->cb_data);
		xdm->last_event_time = ms_time;
	}
	if (xdm->error_count >= XMODEM_MAX_ERRORS) {
		xdm->state = XMODEM_STATE_FAILURE;
		xdm->tx_byte(xdm, XMODEM_CAN, xdm->cb_data);
		xdm->last_event_time = ms_time;
	}
	if (xdm->state != XMODEM_STATE_PROCESS_PACKET)
		return 0;
	xdm->last_event_time = ms_time;
	memcpy(packet, xdm->packet_data, xdm->packet_size);
	*block_num = xdm->block_num;
	xdm->block_num++;
	xdm->state = XMODEM_STATE_SOH;
	xdm->tx_byte(xdm, XMODEM_ACK, xdm->cb_data);
	return xdm->packet_size;
}

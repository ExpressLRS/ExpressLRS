/**
 * @file    xmodem.h
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module is the implementation of the Xmodem protocol.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#ifndef XMODEM_H_
#define XMODEM_H_

#include "uart.h"
#include "flash.h"
#include "stdbool.h"

/* Xmodem (128 bytes) packet format
 * Byte  0:       Header
 * Byte  1:       Packet number
 * Byte  2:       Packet number complement
 * Bytes 3-130:   Data
 * Bytes 131-132: CRC
 */

/* Xmodem (1024 bytes) packet format
 * Byte  0:         Header
 * Byte  1:         Packet number
 * Byte  2:         Packet number complement
 * Bytes 3-1026:    Data
 * Bytes 1027-1028: CRC
 */

/* Maximum allowed errors (user defined). */
#define X_MAX_ERRORS ((uint8_t)3u)

/* Sizes of the packets. */
#define X_PACKET_NUMBER_SIZE  ((uint16_t)2u)
#define X_PACKET_128_SIZE     ((uint16_t)128u)
#define X_PACKET_1024_SIZE    ((uint16_t)1024u)
#define X_PACKET_CRC_SIZE     ((uint16_t)2u)

/* Indexes inside packets. */
#define X_PACKET_NUMBER_INDEX             ((uint16_t)0u)
#define X_PACKET_NUMBER_COMPLEMENT_INDEX  ((uint16_t)1u)
#define X_PACKET_CRC_HIGH_INDEX           ((uint16_t)0u)
#define X_PACKET_CRC_LOW_INDEX            ((uint16_t)1u)

/* Bytes defined by the protocol. */
#define X_SOH ((uint8_t)0x01u)  /**< Start Of Header (128 bytes). */
#define X_STX ((uint8_t)0x02u)  /**< Start Of Header (1024 bytes). */
#define X_EOT ((uint8_t)0x04u)  /**< End Of Transmission. */
#define X_ACK ((uint8_t)0x06u)  /**< Acknowledge. */
#define X_NAK ((uint8_t)0x15u)  /**< Not Acknowledge. */
#define X_CAN ((uint8_t)0x18u)  /**< Cancel. */
#define X_C   ((uint8_t)0x43u)  /**< ASCII "C" to notify the host we want to use CRC16. */

/* Status report for the functions. */
typedef enum {
  X_OK            = 0x00u, /**< The action was successful. */
  X_ERROR_CRC     = 0x01u, /**< CRC calculation error. */
  X_ERROR_NUMBER  = 0x02u, /**< Packet number mismatch error. */
  X_ERROR_UART    = 0x04u, /**< UART communication error. */
  X_ERROR_FLASH   = 0x08u, /**< Flash related error. */
  X_ERROR         = 0xFFu  /**< Generic error. */
} xmodem_status;

void xmodem_receive(void);

#endif /* XMODEM_H_ */

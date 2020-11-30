/**
 * @file    xmodem.c
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module is the implementation of the Xmodem protocol.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#include "xmodem.h"
#include "main.h"

/* Global variables. */
static uint8_t xmodem_packet_number; /**< Packet number counter. */
static uint32_t xmodem_actual_flash_address; /**< Address where we have to write. */
static uint8_t x_first_packet_received; /**< First packet or not. */

/* Local functions. */
static uint16_t xmodem_calc_crc(uint8_t const* data, uint16_t length);
static xmodem_status xmodem_handle_packet(uint8_t size);
static xmodem_status xmodem_error_handler(uint8_t * const error_number);

static bool ledState;
static void blink_led(void)
{
  led_state_set(ledState ? LED_FLASHING : LED_FLASHING_ALT);
  ledState = !ledState;
}

/**
 * @brief   This function is the base of the Xmodem protocol.
 *          When we receive a header from UART, it decides what action it shall
 * take.
 * @param   void
 * @return  void
 */
void xmodem_receive(void) {
  xmodem_status status = X_OK;
  uint8_t error_number = 0u, header;

  x_first_packet_received = false;
  xmodem_packet_number = 1u;
  xmodem_actual_flash_address = FLASH_APP_START_ADDRESS;

  /* Loop until there isn't any error (or until we jump to the user
   * application). */
  while (X_OK == status) {
    header = 0x00u;

    /* Get the header from UART. */
    if (UART_OK != uart_receive_timeout(&header, 1u, 500u)) {
      /* Spam the host (until we receive something) with ACSII "C", to notify it,
      * we want to use CRC-16. */
      if (false == x_first_packet_received) {
        (void)uart_transmit_ch(X_C);
        blink_led();
      }
      /* Uart timeout or any other errors. */
      else {
        status = xmodem_error_handler(&error_number);
      }
      continue;
    }

    /* The header can be: SOH, STX, EOT and CAN. */
    switch (header) {
      /* 128 or 1024 bytes of data. */
      case X_SOH:
      case X_STX: {
        xmodem_status packet_status = xmodem_handle_packet(header);
        if (X_OK != packet_status) {
          /* If the error was flash related, then immediately set the error counter
          * to max (graceful abort).
          */
          if (X_ERROR_FLASH == packet_status) {
            error_number = X_MAX_ERRORS;
          }
          /* Error while processing the packet, either send a NAK or do graceful abort. */
          status = xmodem_error_handler(&error_number);
        }
        break;
      }
      /* End of Transmission. */
      case X_EOT:
        if (x_first_packet_received) {
          /* ACK, feedback to user (as a text), then jump to user application. */
          (void)uart_transmit_ch(X_ACK);
          flash_jump_to_app();
        }
        status = X_ERROR; // Restart sequence
        break;
      /* Abort from host. */
      case X_CAN:
        status = X_ERROR;
        break;
      default:
        /* Wrong header. */
        status = xmodem_error_handler(&error_number);
        break;
    }
  }
}

/**
 * @brief   Calculates the CRC-16 for the input package.
 * @param   *data:  Array of the data which we want to calculate.
 * @param   length: Size of the data, either 128 or 1024 bytes.
 * @return  status: The calculated CRC.
 */
static uint16_t xmodem_calc_crc(uint8_t const* data, uint16_t length) {
  uint16_t crc = 0u;

  if ((xmodem_packet_number & 3) == 0) {
    blink_led();
  }

  while (length) {
    length--;
    crc = crc ^ ((uint16_t)*data++ << 8u);
    for (uint8_t i = 0u; i < 8u; i++) {
      if (crc & 0x8000u) {
        crc = (crc << 1u) ^ 0x1021u;
      } else {
        crc = crc << 1u;
      }
    }
  }
  return crc;
}

/* 2 bytes for packet number, 1024 for data, 2 for CRC*/
uint8_t received_packet_number[X_PACKET_NUMBER_SIZE];
uint8_t received_packet_data[X_PACKET_1024_SIZE];
uint8_t received_packet_crc[X_PACKET_CRC_SIZE];

/**
 * @brief   This function handles the data packet we get from the xmodem
 * protocol.
 * @param   header: SOH or STX.
 * @return  status: Report about the packet.
 */
static xmodem_status xmodem_handle_packet(uint8_t const header)
{
  uint16_t size = 0u, crc_received, crc_calculated;

  /* Get the size of the data. */
  if (X_SOH == header)
  {
    size = X_PACKET_128_SIZE;
  }
  else if (X_STX == header)
  {
    size = X_PACKET_1024_SIZE;
  }
  else
  {
    /* Wrong header type. This shoudn't be possible... */
    return X_ERROR;
  }

  /* Get the packet number, data and CRC from UART. */
  #define TIMEOUT 100 // 100ms
  if (UART_OK != uart_receive_timeout(&received_packet_number[0u], sizeof(received_packet_number), TIMEOUT))
    return X_ERROR_UART;
  if (UART_OK != uart_receive_timeout(&received_packet_data[0u], size, 10))
    return X_ERROR_UART;
  if (UART_OK != uart_receive_timeout(&received_packet_crc[0u], sizeof(received_packet_crc), TIMEOUT))
    return X_ERROR_UART;

  /* Merge the two bytes of CRC. */
  crc_received = (((uint16_t)received_packet_crc[X_PACKET_CRC_HIGH_INDEX] << 8u) |
                  ((uint16_t)received_packet_crc[X_PACKET_CRC_LOW_INDEX]));
  /* We calculate it too. */
  crc_calculated = xmodem_calc_crc(&received_packet_data[0u], size);

  /* error handling. */
  if (xmodem_packet_number != received_packet_number[0u])
  {
    /* Packet number counter mismatch. */
    return X_ERROR_NUMBER;
  }
  if (255u != (received_packet_number[X_PACKET_NUMBER_INDEX] + received_packet_number[X_PACKET_NUMBER_COMPLEMENT_INDEX]))
  {
    /* The sum of the packet number and packet number complement aren't 255. */
    /* The sum always has to be 255. */
    return X_ERROR_NUMBER;
  }
  if (crc_calculated != crc_received)
  {
    /* The calculated and received CRC are different. */
    return X_ERROR_CRC;
  }

  /* If it is the first packet, then erase the memory. */
  if (false == x_first_packet_received)
  {
    if (FLASH_OK != flash_erase(FLASH_APP_START_ADDRESS))
    {
      return X_ERROR_FLASH;
    }
    x_first_packet_received = true;
  }

  /* Do the actual flashing (if there weren't any errors). */
  if (FLASH_OK != flash_write(xmodem_actual_flash_address,
                              (uint32_t*)&received_packet_data[0u],
                              (uint32_t)size / 4u))
  {
    /* Flashing error. */
    return X_ERROR_FLASH;
  }

  /* Raise the packet number and the address counters (if there weren't any errors). */
  xmodem_packet_number++;
  xmodem_actual_flash_address += size;

  /* the handling was successful, then send an ACK. */
  (void)uart_transmit_ch(X_ACK);
  return X_OK;
}

/**
 * @brief   Handles the xmodem error.
 *          Raises the error counter, then if the number of the errors reached
 *          critical, do a graceful abort, otherwise send a NAK.
 * @param   *error_number:    Number of current errors (passed as a pointer).
 * @return  status: X_ERROR in case of too many errors, X_OK otherwise.
 */
static xmodem_status xmodem_error_handler(uint8_t * const error_number)
{
  /* Raise the error counter. */
  (*error_number)++;
  /* If the counter reached the max value, then abort. */
  if ((*error_number) >= X_MAX_ERRORS) {
    /* Graceful abort. */
    (void)uart_transmit_ch(X_CAN);
    (void)uart_transmit_ch(X_CAN);
    return X_ERROR;
  }

  /* Otherwise send a NAK for a repeat. */
  (void)uart_transmit_ch(X_NAK);
  return X_OK;
}

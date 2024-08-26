/*
 * SPDX-FileCopyrightText: 2016 Cesanta Software Limited
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * SPDX-FileContributor: 2016-2022 Espressif Systems (Shanghai) CO LTD
 */

/*
 * Main flasher stub logic
 *
 * This stub uses the same SLIP framing and basic command/response structure
 * as the in-ROM flasher program, but with some enhanced
 * functions and also standardizes the flasher features between different chips.
 *
 * Actual command handlers are implemented in stub_commands.c
 */

#if defined(PLATFORM_ESP32) && defined(TARGET_RX)

#include "stub_flasher.h"
#include "slip.h"
#include "soc_support.h"
#include "stub_commands.h"
#include "stub_write_flash.h"
#include "targets.h"
#include <stdlib.h>

/* Buffers for reading from UART. Data is read double-buffered, so
   we can read into one buffer while handling data from the other one
   (used for flashing throughput.) */
typedef struct
{
    uint8_t *reading_buf;
    uint16_t read; /* how many bytes have we read in the frame */
    slip_state_t state;
} uart_buf_t;
static uart_buf_t ub;

static bool need_reboot = false;
static void  (*flash_method)(uint8_t *, uint32_t) = handle_flash_data;

/* esptool protcol "checksum" is XOR of 0xef and each byte of
   data payload. */
static uint8_t calculate_checksum(const uint8_t *buf, int length)
{
    uint8_t res = 0xEF;
    for (int i = 0; i < length; i++)
    {
        res ^= buf[i];
    }
    return res;
}

static esp_command_error verify_data_len(esp_command_req_t *command, uint8_t len)
{
    return (command->data_len == len) ? ESP_UPDATE_OK : ESP_BAD_DATA_LEN;
}

static void execute_command()
{
    esp_command_req_t *command = (esp_command_req_t *)ub.reading_buf;
    /* provide easy access for 32-bit data words */
    uint32_t *data_words = (uint32_t *)command->data_buf;

    /* Send command response header */
    esp_command_response_t resp = {
        .resp = 1,
        .op_ret = command->op,
        .len_ret = 4, /* esptool.py checks this length */
        .value = 0,
    };


    /* Some commands need to set resp.len_ret or resp.value before it is sent back */
    switch (command->op)
    {
    case ESP_READ_REG:
        if (command->data_len == 4)
        {
            resp.value = READ_REG(data_words[0]);
        }
        break;
    case ESP_FLASH_VERIFY_MD5:
        resp.len_ret = 16 + 2; /* Will sent 16 bytes of data with MD5 value */
        break;
#if defined(HAS_SECURITY_INFO)
    case ESP_GET_SECURITY_INFO:
        resp.len_ret = SECURITY_INFO_BYTES; /* Buffer size varies */
        break;
#endif
    default:
        break;
    }

    /* Send the command response */
    SLIP_send_frame_delimiter();
    SLIP_send_frame_data_buf(&resp, sizeof(esp_command_response_t));

    if (command->data_len > MAX_WRITE_BLOCK + 16)
    {
        SLIP_send_frame_data(ESP_BAD_DATA_LEN);
        SLIP_send_frame_data(0xEE);
        SLIP_send_frame_delimiter();
        Serial.flush(true);
        return;
    }

    /* ... ESP_FLASH_VERIFY_MD5 and ESP_GET_SECURITY_INFO will insert
     * in-frame response data between here and when we send the
     * status bytes at the end of the frame */

    esp_command_error status = ESP_UPDATE_OK;

    /* First stage of command processing - before sending error/status */
    switch (command->op)
    {
    case ESP_SYNC:
        /* Bootloader responds to the SYNC request with eight identical SYNC responses. Stub flasher should react
         * the same way so SYNC could be possible with the flasher stub as well. This helps in cases when the chip
         * cannot be reset and the flasher stub keeps running. */
        status = verify_data_len(command, 36);
        if (status == ESP_UPDATE_OK)
        {
            /* resp.value remains 0 which esptool.py can use to detect the flasher stub */
            resp.value = 0;
            for (int i = 0; i < 7; ++i)
            {
                SLIP_send_frame_status(status);
                SLIP_send_frame_delimiter(); /* end the previous frame */

                SLIP_send_frame_delimiter(); /* start new frame */
                SLIP_send_frame_data_buf(&resp, sizeof(esp_command_response_t));
            }
            /* The last frame is ended outside of the "switch case" at the same place regular one-response frames are
               ended. */
        }
        break;
#if defined(HAS_SECURITY_INFO)
    case ESP_GET_SECURITY_INFO:
        status = verify_data_len(command, 0) ;
        if (status == ESP_UPDATE_OK)
        {
            status = handle_get_security_info();
        }
        break;
#endif
    case ESP_SET_BAUD:
        status = verify_data_len(command, 8);
        break;
    case ESP_READ_FLASH:
        status = verify_data_len(command, 16);
        break;
    case ESP_FLASH_VERIFY_MD5:
        status = verify_data_len(command, 16);
        if (status == ESP_UPDATE_OK)
        {
            status = handle_flash_get_md5sum(data_words[0], data_words[1]);
        }
        break;
    case ESP_FLASH_BEGIN:
        /* parameters (interpreted differently to ROM flasher):
            0 - erase_size (used as total size to write)
            1 - num_blocks (ignored)
            2 - block_size (should be MAX_WRITE_BLOCK, relies on num_blocks * block_size >= erase_size)
            3 - offset (used as-is)
        */
        if (command->data_len == 16 && data_words[2] > MAX_WRITE_BLOCK)
        {
            status = ESP_BAD_BLOCKSIZE;
            break;
        }
        status = verify_data_len(command, 16);
        if (status != ESP_UPDATE_OK)
        {
            break;
        }
        status = handle_flash_begin(data_words[0], data_words[3]);
        need_reboot = data_words[0] == 0;
        flash_method = handle_flash_data;
        break;
    case ESP_FLASH_DEFLATED_BEGIN:
        /* parameters:
            0 - uncompressed size
            1 - num_blocks (based on compressed size)
            2 - block_size (should be MAX_WRITE_BLOCK, total bytes over serial = num_blocks * block_size)
            3 - offset (used as-is)
        */
        if (command->data_len == 16 && data_words[2] > MAX_WRITE_BLOCK)
        {
            status = ESP_BAD_BLOCKSIZE;
            break;
        }
        status = verify_data_len(command, 16);
        if (status != ESP_UPDATE_OK)
        {
            break;
        }
        status = handle_flash_deflated_begin(data_words[0], data_words[1] * data_words[2], data_words[3]);
        need_reboot = data_words[0] == 0;
        flash_method = handle_flash_deflated_data;
        break;
    case ESP_FLASH_DATA:
    case ESP_FLASH_DEFLATED_DATA:
        if (!is_in_flash_mode())
        {
            status = ESP_NOT_IN_FLASH_MODE;
            break;
        }
        status = get_flash_error();
        if (status != ESP_UPDATE_OK)
        {
            break;
        }
        if (data_words[0] != command->data_len - 16)
        {
            status = ESP_BAD_DATA_LEN;
            break;
        }
        if (calculate_checksum(command->data_buf + 16, command->data_len - 16) != command->checksum)
        {
            status = ESP_BAD_DATA_CHECKSUM;
            break;
        }
        /* drop into flashing mode, discard 16 byte payload header */
        flash_method(command->data_buf + 16, command->data_len - 16);
        break;
    case ESP_FLASH_END:
    case ESP_FLASH_DEFLATED_END:
        status = handle_flash_end();
        break;
    case ESP_READ_REG:
        status = verify_data_len(command, 4);
        break;
    case ESP_FLASH_ENCRYPT_DATA:
    case ESP_MEM_BEGIN:
    case ESP_MEM_DATA:
    case ESP_MEM_END:
        status = ESP_CMD_NOT_IMPLEMENTED;
        break;
    case ESP_RUN_USER_CODE:
        ESP.restart();
        return;
    }

    SLIP_send_frame_status(status);
    SLIP_send_frame_delimiter();
    Serial.flush(true);

    if (status == ESP_UPDATE_OK && (command->op == ESP_FLASH_DEFLATED_END || command->op == ESP_FLASH_END))
    {
        /* passing 0 as parameter for ESP_FLASH_END means reboot now, or the begin was passed 0 size so use that as a reboot too */
        if (data_words[0] == 0 || need_reboot)
        {
            delayMicroseconds(10000);
            ESP.restart();
        }
    }
}

void start_esp_upload()
{
    ub.reading_buf = static_cast<uint8_t *>(malloc(32768 + 64));
}

void stub_handle_rx_byte(char byte)
{
    int16_t r = SLIP_recv_byte(byte, (slip_state_t *)&ub.state);
    if (r >= 0)
    {
        ub.reading_buf[ub.read++] = (uint8_t)r;
        if (ub.read == MAX_WRITE_BLOCK + 64)
        {
            r = SLIP_FINISHED_FRAME;
        }
    }
    if (r == SLIP_FINISHED_FRAME)
    {
        execute_command();
        ub.read = 0;
    }
}
#endif
#include "frsky.h"
#include "uart.h"
#include "main.h"
#include "flash.h"

#include <string.h>

#define FRAME_SIZE 8
#define MAX_FRAME_SIZE 19

#define RX_PACKET_SIZE 0x09

#define HEADBYTE 0x50

#define START_STOP 0x7E
#define BYTE_STUFF 0x7D
#define STUFF_MASK 0x20

#define TX_BYTE 0xFF // received from TX
#define RX_BYTE 0x5E // send to TX

#define RX_CRC(crc) (((crc) >> 8) & 0xff)
#define TX_CRC(crc) ((crc)&0xff)

//#define INVERT(_b) (~(_b))
#define INVERT(_b) (_b)

#define FRSKY_HEADER_SIZE 16

enum
{
    PRIM_REQ_POWERUP = 0x0,
    PRIM_REQ_VERSION = 0x01,
    PRIM_CMD_DOWNLOAD = 0x03,
    PRIM_DATA_WORD = 0x04,
    PRIM_DATA_EOF = 0x05
};

enum
{
    PRIM_ACK_POWERUP = 0x80,
    PRIM_ACK_VERSION = 0x81,
    PRIM_REQ_DATA_ADDR = 0x82,
    PRIM_END_DOWNLOAD = 0x83,
    PRIM_DATA_CRC_ERR = 0x84
};

enum rx_state
{
    STATE_DATA_IDLE = 0x01,
    STATE_DATA_IN_FRAME = 0x02,
    STATE_DATA_XOR = 0x03,
};

static uint_fast8_t flash_ongoing = 0;
static uint32_t address_offset = 0;

/* frame[0..6 = data][7 = crc] */
uint8_t frame[FRAME_SIZE];

uint16_t crc16(const uint8_t *data_p, uint8_t length)
{
    uint8_t tmp;
    uint16_t crc = 0;

    while (length--)
    {
        tmp = crc >> 8 ^ *data_p++;
        tmp ^= tmp >> 4;
        crc = (crc << 8) ^ ((uint16_t)(tmp << 12)) ^ ((uint16_t)(tmp << 5)) ^ ((uint16_t)tmp);
    }
    return crc;
}

uint8_t *startFrame(const uint8_t command)
{
    memset(frame, 0, sizeof(frame));
    //frame[0] = START_STOP;
    //frame[1] = RX_BYTE;
    //frame[2] = HEADBYTE;
    //frame[3] = command;
    //return &frame[4];
    frame[0] = HEADBYTE;
    frame[1] = command;
    return &frame[2];
}

void send_frame(void)
{
    uint8_t i;
    uint8_t buff[12]; // TODO: merge this with frame
    uint8_t *ptr = buff;
    *ptr++ = START_STOP;
    *ptr++ = RX_BYTE;

    frame[7] = RX_CRC(crc16(frame, 7));

    for (i = 0; i < FRAME_SIZE; i++)
    {
        if (frame[i] == START_STOP || frame[i] == BYTE_STUFF)
        {
            *ptr++ = INVERT(BYTE_STUFF);
            *ptr++ = INVERT(STUFF_MASK ^ frame[i]);
        }
        else
        {
            *ptr++ = INVERT(frame[i]);
        }
    }
    uart_transmit_bytes(buff, (ptr - buff));
}

void send_command(const uint8_t command)
{
    (void)startFrame(command);
    send_frame();
}

void send_address(void)
{
    uint8_t *ptr = startFrame(PRIM_REQ_DATA_ADDR);
    *((uint32_t *)ptr) = address_offset;
    send_frame();
}

uint8_t check_crc(const uint8_t first)
{
    uint16_t crc;
    if (first && frame[7] == 0xff)
        return 1;
    crc = crc16(frame, 7);
    return (TX_CRC(crc) == frame[7]);
}

void process_frame(const uint8_t first)
{
    /* Check CRC */
    if (check_crc(first))
    {
        /* Check head byte */
        if (frame[0] != HEADBYTE)
        {
            return;
        }

        /* Check the command */
        switch (frame[1])
        {
        case PRIM_REQ_POWERUP:
            send_command(PRIM_ACK_POWERUP);
            break;
        case PRIM_REQ_VERSION:
            send_command(PRIM_ACK_VERSION);
            flash_ongoing = 1;
            break;
        case PRIM_CMD_DOWNLOAD:
            // start upload, give file offset
            address_offset = 0;
            send_address();
            break;
        case PRIM_DATA_WORD:
        {
            /* Check that address is correct */
            if (frame[6] == (address_offset & 0xff))
            {
                if (FRSKY_HEADER_SIZE <= address_offset)
                {
                    uint32_t data = *((uint32_t *)(&frame[2]));
                    uint32_t tgt_addr = FLASH_APP_START_ADDRESS +
                                        (address_offset - FRSKY_HEADER_SIZE);
                    if ((tgt_addr & (FLASH_PAGE_SIZE - 1)) == 0)
                        flash_erase_page(tgt_addr);
                    if (flash_write(tgt_addr, &data, 1) != FLASH_OK)
                        return;
                }
                address_offset += 4;
                send_address();
            }
            break;
        }
        case PRIM_DATA_EOF:
            send_command(PRIM_END_DOWNLOAD);
            flash_ongoing = 0;
            break;
        default:
            return;
        }
    }
    else
    {
        send_command(PRIM_DATA_CRC_ERR);
    }
}

void readFrame(void)
{
    uint8_t *frame_ptr = frame;
    uint8_t data, rx_state, len;

start_read:
    rx_state = STATE_DATA_IN_FRAME;
    // check tx byte
    if (uart_receive_timeout(&data, 1, 10) != UART_OK ||
        INVERT(data) != TX_BYTE)
    {
        goto exit_read;
    }

    while (uart_receive(&data, 1) == UART_OK)
    {
        data = INVERT(data);

        if (data == START_STOP)
        {
            /* Frame start detected, restart... */
            goto start_read;
        }
        else if (rx_state == STATE_DATA_IN_FRAME &&
                 data == BYTE_STUFF)
        {
            rx_state = STATE_DATA_XOR;
        }
        else if (rx_state == STATE_DATA_XOR)
        {
            *frame_ptr++ = data ^ STUFF_MASK;
            rx_state = STATE_DATA_IN_FRAME;
        }
        else
        {
            *frame_ptr++ = data;
        }

        len = (frame_ptr - frame);

        if (len == FRAME_SIZE ||
            (len == 7 && data == 0xff))
        {
            process_frame((data == 0xff));
            goto exit_read;
        }
    }
exit_read:
    return;
}

int8_t frsky_check(void)
{
    uint8_t led_state = 1;
    uint8_t data;

    while (1)
    {
        data = 0;
        if (uart_receive_timeout(&data, 1, 100) == UART_OK)
        {
            if (INVERT(data) == START_STOP)
            {
                led_state_set(led_state ? LED_FLASHING : LED_FLASHING_ALT);
                led_state ^= 1;
                /* frame start detected capture rest and process */
                readFrame();
            }
            else if (!flash_ongoing && boot_wait_timer_end())
            {
                goto exit_frsky;
            }
        }
        else if (!flash_ongoing && boot_wait_timer_end())
        {
            goto exit_frsky;
        }
    }

exit_frsky:
    return -1;
}

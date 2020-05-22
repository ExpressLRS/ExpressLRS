/*
 * stk500 bootloader code.
 *
 * Thanks to MikeBland for the initial code.
 * https://github.com/MikeBland/StmMultiBoot
 */

#include "stk500.h"
#include "flash.h"
#include "uart.h"
#include "main.h"

#define OPTIBOOT_MAJVER 4
#define OPTIBOOT_MINVER 5

uint16_t Buff[256];
uint8_t insync;

uint8_t getch(void)
{
  uint8_t ch = 0;
  if (uart_receive_timeout(&ch, 1u, 100) == UART_ERROR)
    return UART_ERROR;
  return ch;
}

void verifySpace(void)
{
  if (getch() != CRC_EOP)
  {
    insync = 0;
    return;
  }
  uart_transmit_ch(STK_INSYNC);
  insync = 1;
}

void bgetNch(uint8_t count)
{
  do
  {
    getch();
  } while (--count);
  verifySpace();
}

static int8_t stk500_update(void)
{
  uint32_t address = 0;
  uint8_t ch, GPIOR0, led = 1;
  int8_t retval;

  insync = 0;
  led_red_state_set(led);

  for (retval = 0; retval == 0;)
  {
    /* get character from UART */
    if (uart_receive_timeout(&ch, 1u, 5) == UART_ERROR)
    {
      if (timer_end())
        return -1;
      continue;
    }

    led ^= 1;
    led_red_state_set(led);

    if (ch == STK_GET_SYNC)
    {
      verifySpace();
      if (insync)
        led_green_state_set(1);
    }
    else if (ch == STK_GET_PARAMETER)
    {
      GPIOR0 = getch();
      verifySpace();
      if (GPIOR0 == 0x82)
      {
        uart_transmit_ch(OPTIBOOT_MINVER);
      }
      else if (GPIOR0 == 0x81)
      {
        uart_transmit_ch(OPTIBOOT_MAJVER);
      }
      else
      {
        /*
         * GET PARAMETER returns a generic 0x03 reply for
         * other parameters - enough to keep Avrdude happy
         */
        uart_transmit_ch(0x03);
      }
    }
    else if (ch == STK_SET_DEVICE)
    {
      // SET DEVICE is ignored
      bgetNch(20);
    }
    else if (ch == STK_SET_DEVICE_EXT)
    {
      // SET DEVICE EXT is ignored
      bgetNch(5);
    }
    else if (ch == STK_LOAD_ADDRESS)
    {
      // LOAD ADDRESS
      uint16_t newAddress;
      newAddress = getch();
      newAddress = (newAddress & 0xff) | (getch() << 8);
      address = newAddress; // Convert from word address to byte address
      address <<= 1;
      verifySpace();
    }
    else if (ch == STK_UNIVERSAL)
    {
      // UNIVERSAL command is ignored
      bgetNch(4);
      uart_transmit_ch(0x00);
    }
    else if (ch == STK_PROG_PAGE)
    {
      // PROGRAM PAGE - we support flash programming only, not EEPROM
      uint8_t *bufPtr;
      uint16_t page_size; // page_size
      uint16_t count;
      uint8_t *memAddress;
      // read page size, 2 bytes
      page_size = getch() << 8; /* getlen() */
      page_size |= getch();
      getch(); // discard flash/eeprom byte
      // While that is going on, read in page contents
      count = page_size;
      bufPtr = (uint8_t *)Buff;
      do
      {
        *bufPtr++ = getch();
      } while (--count);
      if (page_size & 1)
      {
        *bufPtr = 0xFF;
      }
      count = page_size;
      count += 1;
      count /= 2;
      memAddress = (uint8_t *)(address + FLASH_APP_START_ADDRESS);

      // Read command terminator, start reply
      verifySpace();

      if ((uint32_t)memAddress < FLASH_BANK1_END)
      {
        if ((uint32_t)memAddress >= FLASH_APP_START_ADDRESS)
        {
          if (((uint32_t)memAddress & (FLASH_PAGE_SIZE - 1)) == 0)
          {
            // At page start so erase it
            flash_erase((uint32_t)memAddress);
          }
          flash_write_halfword((uint32_t)memAddress, Buff, count);
        }
      }
    }
    else if (ch == STK_READ_PAGE)
    {
      uint16_t length;
      uint8_t xlen;
      uint8_t *memAddress;
      memAddress = (uint8_t *)(address + FLASH_BASE);
      // READ PAGE - we only read flash
      xlen = getch(); /* getlen() */
      length = getch() | (xlen << 8);
      getch();
      verifySpace();
      do
      {
        uart_transmit_ch(*memAddress++);
      } while (--length);
    }
    else if (ch == STK_READ_SIGN)
    {
      // READ SIGN - return what opentx wants to hear
      verifySpace();
      uart_transmit_ch(SIGNATURE_0);
      uart_transmit_ch(SIGNATURE_1);
      uart_transmit_ch(SIGNATURE_2);
    }
    else if (ch == STK_LEAVE_PROGMODE)
    { /* 'Q' */
      // Adaboot no-wait mod
      verifySpace();
      retval = -1; // flash end, boot to app
    }
#if 0
    else
    {
      // This covers the response to commands like STK_ENTER_PROGMODE
      verifySpace();
    }
#endif

    if (insync)
      uart_transmit_ch(STK_OK);
  }

  return retval;
}

int8_t stk500_check(void)
{
#if 0
  int8_t ret = -1;
  uint8_t in = 0;

  /* seach possible sync requests to start upload */
  for (;;)
  {
    if (uart_receive_timeout(&in, 1u, 10) == UART_ERROR)
    {
      if (timer_end())
        goto exit_check; // timer end, start app
    }
    else if (in == STK_GET_SYNC)
    {
      uart_receive_timeout(&in, 1u, 10);
      if (in == CRC_EOP)
      {
        uart_transmit_ch(STK_INSYNC);
        uart_transmit_ch(STK_OK);
        ret = stk500_update();
        goto exit_check;
      }
    }
  }
exit_check:
  return ret;
#else
  return stk500_update();
#endif
}

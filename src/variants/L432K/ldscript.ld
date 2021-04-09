/*
**  Abstract    : Linker script for STM32L432KCUx Device with
**                256KByte FLASH, 64KByte RAM
*/

/* Specify the memory areas */
MEMORY
{
  RAM_CODE (rx)   : ORIGIN = 0x20000000, LENGTH = 0K
  RAM_DATA (xrw)  : ORIGIN = 0x20000000 + LENGTH(RAM_CODE), LENGTH = 64K - LENGTH(RAM_CODE)
  CCMRAM (rw)     : ORIGIN = 0x10000000, LENGTH = 0K
  FLASH (rx)      : ORIGIN = 0x8000000 + FLASH_APP_OFFSET, LENGTH = 128K - FLASH_APP_OFFSET
}

INCLUDE "variants/ldscript_gen.ld"

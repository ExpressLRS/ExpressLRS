/*
**  Abstract    : Linker script for STM32F303K8Tx Device with
**                64KByte FLASH, 12KByte RAM
*/

/* Specify the memory areas */
MEMORY
{
  RAM_CODE (rx)   : ORIGIN = 0x20000000, LENGTH = 0K
  RAM_DATA (xrw)  : ORIGIN = 0x20000000 + LENGTH(RAM_CODE), LENGTH = 16K - LENGTH(RAM_CODE)
  CCMRAM (rw)     : ORIGIN = 0x10000000, LENGTH = 4K
  FLASH (rx)      : ORIGIN = 0x08000000 + FLASH_APP_OFFSET, LENGTH = 64K - FLASH_APP_OFFSET
}

INCLUDE "variants/ldscript_gen.ld"

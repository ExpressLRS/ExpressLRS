/*
**  Abstract    : Linker script for STM32F103C(8-B)Tx Device with
**                32/64/128KByte FLASH, 10/20KByte RAM
*/

/* Specify the memory areas */
MEMORY
{
RAM_CODE (rx)   : ORIGIN = 0x20000000, LENGTH = 0K
RAM_DATA (xrw)  : ORIGIN = 0x20000000 + LENGTH(RAM_CODE), LENGTH = 20K - LENGTH(RAM_CODE)
CCMRAM (rw)     : ORIGIN = 0x10000000, LENGTH = 0K
FLASH (rx)      : ORIGIN = 0x8000000, LENGTH = 64K
}

INCLUDE "variants/ldscript_gen.ld"

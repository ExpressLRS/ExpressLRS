/*
*****************************************************************************
**

**  Abstract    : Linker script for STM32F303CC Device with
**                256KByte FLASH, 40KByte RAM, 8KByte CCMRAM
**

/* Specify the memory areas */
MEMORY
{
RAM_CODE (rx)   : ORIGIN = 0x20000000, LENGTH = 1K
RAM_DATA (xrw)  : ORIGIN = 0x20000000 + LENGTH(RAM_CODE), LENGTH = 40K - LENGTH(RAM_CODE)
CCMRAM (rw)     : ORIGIN = 0x10000000, LENGTH = 8K
FLASH (rx)      : ORIGIN = 0x8008000, LENGTH = 256K - 0x8000
}

INCLUDE "variants/ldscript_gen.ld"

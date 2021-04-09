/*
  NOTE: Don't use this linker file for any new target!!
*/

/* Specify the memory areas */
MEMORY
{
RAM_CODE (rx)   : ORIGIN = 0x20000000, LENGTH = 0K
RAM_DATA (xrw)  : ORIGIN = 0x20000000, LENGTH = 20K
CCMRAM (rw)     : ORIGIN = 0x10000000, LENGTH = 0K
FLASH (rx)      : ORIGIN = 0x8002000, LENGTH = 128K - 0x2000
}

INCLUDE "variants/ldscript_gen.ld"

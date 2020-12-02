#ifndef Stk500_h
#define Stk500_h

#include <stdint.h>

#define STK_BAUD_RATE       115200
#define STK_SYNC_CTN        30
#define STK_SYNC_TIMEOUT    50
#define STK_TIMEOUT         1000
#define STK_PAGE_SIZE       128

#define SIGNATURE_0 0x1E
#if (STK_PAGE_SIZE == 256)
/* These are causing:
    pageSize = 256;
    writeOffset = 0x1000; // start offset (word address)
*/
#define SIGNATURE_1 0x55
#define SIGNATURE_2 0xAA
#elif (STK_PAGE_SIZE == 128)
/* These are causing:
    pageSize = 128;
    writeOffset = 0; // start offset (word address)
*/
#define SIGNATURE_1 0x97
#define SIGNATURE_2 0x02
#endif

/* STK500 constants list, from AVRDUDE */
#define STK_OK 0x10
#define STK_FAILED 0x11          // Not used
#define STK_UNKNOWN 0x12         // Not used
#define STK_NODEVICE 0x13        // Not used
#define STK_INSYNC 0x14          // ' '
#define STK_NOSYNC 0x15          // Not used
#define ADC_CHANNEL_ERROR 0x16   // Not used
#define ADC_MEASURE_OK 0x17      // Not used
#define PWM_CHANNEL_ERROR 0x18   // Not used
#define PWM_ADJUST_OK 0x19       // Not used
#define CRC_EOP 0x20             // 'SPACE'
#define STK_GET_SYNC 0x30        // '0'
#define STK_GET_SIGN_ON 0x31     // '1'
#define STK_SET_PARAMETER 0x40   // '@'
#define STK_GET_PARAMETER 0x41   // 'A'
#define STK_SET_DEVICE 0x42      // 'B'
#define STK_SET_DEVICE_EXT 0x45  // 'E'
#define STK_ENTER_PROGMODE 0x50  // 'P'
#define STK_LEAVE_PROGMODE 0x51  // 'Q'
#define STK_CHIP_ERASE 0x52      // 'R'
#define STK_CHECK_AUTOINC 0x53   // 'S'
#define STK_LOAD_ADDRESS 0x55    // 'U'
#define STK_UNIVERSAL 0x56       // 'V'
#define STK_PROG_FLASH 0x60      // '`'
#define STK_PROG_DATA 0x61       // 'a'
#define STK_PROG_FUSE 0x62       // 'b'
#define STK_PROG_LOCK 0x63       // 'c'
#define STK_PROG_PAGE 0x64       // 'd'
#define STK_PROG_FUSE_EXT 0x65   // 'e'
#define STK_READ_FLASH 0x70      // 'p'
#define STK_READ_DATA 0x71       // 'q'
#define STK_READ_FUSE 0x72       // 'r'
#define STK_READ_LOCK 0x73       // 's'
#define STK_READ_PAGE 0x74       // 't'
#define STK_READ_SIGN 0x75       // 'u'
#define STK_READ_OSCCAL 0x76     // 'v'
#define STK_READ_FUSE_EXT 0x77   // 'w'
#define STK_READ_OSCCAL_EXT 0x78 // 'x'

uint8_t stk500_write_file(const char *filename);

#endif

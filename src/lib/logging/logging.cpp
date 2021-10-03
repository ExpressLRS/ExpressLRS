#include <Arduino.h>
#include <cstdarg>
#include "logging.h"

#ifdef LOG_USE_PROGMEM
  #define GETCHAR pgm_read_byte(fmt)
#else
  #define GETCHAR *fmt
#endif

void debugPrintf(const char* fmt, ...)
{
  char c;
  va_list  vlist;
  va_start(vlist,fmt);

  c = GETCHAR;
  while(c) {
    if (c == '%') {
      fmt++;
      c = GETCHAR;
      switch (c) {
        case 's':
          LOGGING_UART.print(va_arg(vlist,const char *));
          break;
        case 'd':
          LOGGING_UART.print(va_arg(vlist,int32_t), DEC);
          break;
        case 'u':
          LOGGING_UART.print(va_arg(vlist,uint32_t), DEC);
          break;
        case 'x':
          LOGGING_UART.print(va_arg(vlist,uint32_t), HEX);
          break;
        default:
          break;
      }
    } else {
      LOGGING_UART.write(c);
    }
    fmt++;
    c = GETCHAR;
  }
  va_end(vlist);
}
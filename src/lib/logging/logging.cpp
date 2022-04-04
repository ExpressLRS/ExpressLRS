#include "targets.h"
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
  const char *v;
  char buf[11];
  va_list  vlist;
  va_start(vlist,fmt);

  c = GETCHAR;
  while(c) {
    if (c == '%') {
      fmt++;
      c = GETCHAR;
      v = buf;
      buf[0] = 0;
      switch (c) {
        case 's':
          v = va_arg(vlist, const char *);
          break;
        case 'd':
          itoa(va_arg(vlist, int32_t), buf, DEC);
          break;
        case 'u':
          utoa(va_arg(vlist, uint32_t), buf, DEC);
          break;
        case 'x':
          utoa(va_arg(vlist, uint32_t), buf, HEX);
          break;
        default:
          break;
      }
      LOGGING_UART.write((uint8_t*)v, strlen(v));
    } else {
      LOGGING_UART.write(c);
    }
    fmt++;
    c = GETCHAR;
  }
  va_end(vlist);
}
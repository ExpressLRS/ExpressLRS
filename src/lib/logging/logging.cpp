#include "targets.h"
#include <cstdarg>
#include "logging.h"

#ifdef LOG_USE_PROGMEM
  #define GETCHAR pgm_read_byte(fmt)
#else
  #define GETCHAR *fmt
#endif

Stream *BackpackOrLogStrm;

void debugPrintf(const char* fmt, ...)
{
  char c;
  const char *v;
  char buf[21];
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
        case 'f':
          {
            float val = va_arg(vlist, double);
            itoa((int32_t)val, buf, DEC);
            strcat(buf, ".");
            int32_t decimals = abs((int32_t)(val * 1000)) % 1000;
            itoa(decimals, buf + strlen(buf), DEC);
          }
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

#if defined(DEBUG_INIT)
// Create a UART to send DBGLN to during preinit
void debugCreateInitLogger()
{
  #if defined(PLATFORM_ESP32)
  BackpackOrLogStrm = new HardwareSerial(1);
  ((HardwareSerial *)BackpackOrLogStrm)->begin(460800, SERIAL_8N1, 3, 1);
  #else
  BackpackOrLogStrm = new HardwareSerial(0);
  ((HardwareSerial *)BackpackOrLogStrm)->begin(460800, SERIAL_8N1);
  #endif
}

void debugFreeInitLogger()
{
  ((HardwareSerial *)BackpackOrLogStrm)->end();
  delete (HardwareSerial *)BackpackOrLogStrm;
  BackpackOrLogStrm = nullptr;
}
#endif

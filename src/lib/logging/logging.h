#ifndef DEBUG_H
#define DEBUG_H

#ifndef LOGGING_UART
#define LOGGING_UART Serial
#endif

// #define LOG_USE_PROGMEM

extern void debugPrintf(const char* fmt, ...);

#define INFOLN(msg) LOGGING_UART.println(msg)
#define ERRLN(msg) LOGGING_UART.println("ERROR: " msg)

#ifdef DEBUG_ENABLE
  #define DBGCR   LOGGING_UART.println()
  #ifndef LOG_USE_PROGMEM
    #define DBG(msg, ...)   debugPrintf(msg, ##__VA_ARGS__)
    #define DBGLN(msg, ...) { \
      debugPrintf(msg, ##__VA_ARGS__); \
      LOGGING_UART.println(); \
    }
  #else
    #define DBG(msg, ...)   debugPrintf(PSTR(msg), ##__VA_ARGS__)
    #define DBGLN(msg, ...) { \
      debugPrintf(PSTR(msg), ##__VA_ARGS__); \
      LOGGING_UART.println(); \
    }
  #endif
#else
  #define DBGCR
  #define DBG(...)
  #define DBGLN(...)
#endif

#endif
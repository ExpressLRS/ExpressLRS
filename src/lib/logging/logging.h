#ifndef DEBUG_H
#define DEBUG_H

#include "VA_OPT.h"

/**
 * Debug logging macros. Define DEBUG_LOG or DEBUG_LOG_VERBOSE to enable logging,
 * verbose adds overwhelming detail. All macros start with DBG or DBGV (for verbose)
 * DBGCR / DBGVCR - Print newline (Serial.println())
 * DBG / DBGV - Print messag with optional format specifier (Serial.printf(x, ...))
 * DBGLN / DBGVLN - Same as DBG except also includes newline
 * DBGW / DBGVW - Write a single byte to logging (Serial.write(x))
 *
 * Set LOGGING_UART define to Serial instance to use if not Serial
 **/

// DEBUG_LOG_VERBOSE and DEBUG_RX_SCOREBOARD implies DEBUG_LOG
#if !defined(DEBUG_LOG)
  #if defined(DEBUG_LOG_VERBOSE) || (defined(DEBUG_RX_SCOREBOARD) && TARGET_RX) || defined(DEBUG_INIT)
    #define DEBUG_LOG
  #endif
#endif

#if defined(TARGET_RX) && (defined(DEBUG_RCVR_LINKSTATS) || defined(DEBUG_RX_SCOREBOARD) || defined(DEBUG_RCVR_SIGNAL_STATS)) || defined(DEBUG_LOG)
#define DEBUG_ENABLED
#endif

#if defined(TARGET_TX)
extern Stream *TxBackpack;
#if defined(PLATFORM_ESP32_S3)
#define LOGGING_UART (Serial)
#else
#define LOGGING_UART (*TxBackpack)
#endif
#else
extern Stream *SerialLogger;
#define LOGGING_UART (*SerialLogger)
#endif

// #define LOG_USE_PROGMEM

void debugPrintf(const char* fmt, ...);
#if defined(LOG_INIT)
void debugCreateInitLogger();
void debugFreeInitLogger();
#else
#define debugCreateInitLogger()
#define debugFreeInitLogger()
#endif

#if defined(CRITICAL_FLASH) || ((defined(DEBUG_RCVR_LINKSTATS)) && !defined(DEBUG_LOG))
  #define ERRLN(msg, ...)
#else
  #define ERRLN(msg, ...) IFNE(__VA_ARGS__)({ \
      LOGGING_UART.print("ERROR: "); \
      debugPrintf(msg, ##__VA_ARGS__); \
      LOGGING_UART.println(); \
  },LOGGING_UART.println("ERROR: " msg))
#endif

#if defined(DEBUG_LOG) && !defined(CRITICAL_FLASH)
  #define DBGCR   LOGGING_UART.println()
  #define DBGW(c) LOGGING_UART.write(c)
  #ifndef LOG_USE_PROGMEM
    #define DBG(msg, ...)   debugPrintf(msg, ##__VA_ARGS__)
    #define DBGLN(msg, ...) do { \
      debugPrintf(msg, ##__VA_ARGS__); \
      LOGGING_UART.println(); \
    } while(0)
  #else
    #define DBG(msg, ...)   debugPrintf(PSTR(msg), ##__VA_ARGS__)
    #define DBGLN(msg, ...) { \
      debugPrintf(PSTR(msg), ##__VA_ARGS__); \
      LOGGING_UART.println(); \
    }
  #endif

  // Verbose logging is for spammy stuff
  #if defined(DEBUG_LOG_VERBOSE)
    #define DBGVCR DBGCR
    #define DBGVW(c) DBGW(c)
    #define DBGV(...) DBG(__VA_ARGS__)
    #define DBGVLN(...) DBGLN(__VA_ARGS__)
  #else
    #define DBGVCR
    #define DBGVW(c)
    #define DBGV(...)
    #define DBGVLN(...)
  #endif
#else
  #define DBGCR
  #define DBGW(c)
  #define DBG(...)
  #define DBGLN(...)
  #define DBGVCR
  #define DBGV(...)
  #define DBGVLN(...)
#endif

#endif
#ifndef DEBUG_H_
#define DEBUG_H_

#include <Arduino.h>
#include <HardwareSerial.h>
//#include "targets.h"
#include "HwSerial.h"

#ifndef DEBUG_SERIAL

#if defined(PLATFORM_ESP32)
#define DEBUG_SERIAL Serial
#elif defined(TARGET_R9M_TX)
#define DEBUG_SERIAL Serial
#elif defined(PLATFORM_ESP8266)
#define DEBUG_SERIAL CrsfSerial
//#define DEBUG_SERIAL Serial1 // TX1 as output
#elif defined(TARGET_R9M_RX)
//#define DEBUG_SERIAL CrsfSerial
//#define DEBUG_SERIAL Serial
#endif

#endif // ifndef DEBUG_SERIAL

#ifdef DEBUG_SERIAL
// Debug enabled
#define DEBUG_PRINT(...) DEBUG_SERIAL.print(__VA_ARGS__)
#define DEBUG_PRINTLN(...) DEBUG_SERIAL.println(__VA_ARGS__)
#define DEBUG_PRINTF(...) DEBUG_SERIAL.printf(__VA_ARGS__)
#define DEBUG_WRITE(...) DEBUG_SERIAL.write(__VA_ARGS__)
#else
// Debug disabled
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINTF(...)
#define DEBUG_WRITE(...)
#endif

#endif // DEBUG_H_

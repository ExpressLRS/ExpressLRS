#ifndef DEBUG_H_
#define DEBUG_H_

// Macros for logging debug messages to the serial port
// Assumes that the Serial object has been initialised and setup

// Uncomment the line below to enable debug logging
// This also stops the CRSF object from sending packets to the FC
//#define DEBUG

#ifdef DEBUG
    #define DEBUG_PRINT(...) DEBUG_PRINT(__VA_ARGS__)
    #define DEBUG_PRINTLN(...) DEBUG_PRINTLN(__VA_ARGS__)
    #define DEBUG_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(format, ...)
#endif

#endif // DEBUG_H_
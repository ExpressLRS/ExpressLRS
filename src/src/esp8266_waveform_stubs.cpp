#if defined(PLATFORM_ESP8266)
#include <Arduino.h>
extern "C" IRAM_ATTR int __wrap_stopWaveform(uint8_t pin) { return true; }
extern "C" IRAM_ATTR bool __wrap__stopPWM(uint8_t pin) { return true; }
#endif

#if defined(PLATFORM_ESP8266)
#include <Arduino.h>

// PWM waveform generator is not used, keep its entry points out of the image
extern "C" IRAM_ATTR int __wrap_stopWaveform(uint8_t pin) { return true; }
extern "C" IRAM_ATTR bool __wrap__stopPWM(uint8_t pin) { return true; }

// The core registers DWARF unwind frames in do_global_ctors(), which pulls
// libgcc's unwinder (~4KB) into the image. We build without exceptions and
// never unwind, so satisfy the symbols here instead.
struct object;
extern "C" void __register_frame_info(const void *, struct object *) {}
extern "C" void *__deregister_frame_info(const void *) { return nullptr; }
#endif

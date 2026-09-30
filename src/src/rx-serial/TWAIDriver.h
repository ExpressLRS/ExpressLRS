#pragma once

#if defined(PLATFORM_ESP32)

#include <stdint.h>

// Extended (29-bit) data frame, the only kind DroneCAN uses
struct TWAIFrame {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
};

enum TWAIState : uint8_t {
    TWAI_STOPPED,
    TWAI_RUNNING,
    TWAI_BUS_OFF,
    TWAI_RECOVERING,
    TWAI_RESETTING,     // ESP32 errata reset pending, done by poll()
};

struct TWAIStats {
    uint32_t rxOverrun;     // hardware FIFO (64 bytes) overflows, each drops several frames
    uint32_t rxQueueFull;   // frames lost to the software RX ring being full
    uint32_t txFailed;      // frames the controller gave up on
    uint32_t busOff;
    uint32_t errataResets;  // ESP32 only
};

// Dual acceptance filter on extended ID bits 28..13, set mask bits are don't care
struct TWAIFilter {
    uint16_t code1, mask1;
    uint16_t code2, mask2;
};

/**
 * Minimal TWAI (CAN) driver for 1Mbit/s extended frames with the ISR in IRAM, so frames
 * keep being received while the flash cache is disabled (flash erase/write, OTA).
 */
class TWAIDriver {
public:
    static bool begin(int8_t txPin, int8_t rxPin, const TWAIFilter &filter);
    static void end();

    // Call from the main loop, handles bus-off recovery and a lost TX interrupt
    static void poll();
    static bool receive(TWAIFrame &frame);
    // Returns false if the TX queue is full
    static bool transmit(const TWAIFrame &frame);
    // Drops queued frames and aborts the one being sent
    static void flushTx();
    // Frames acknowledged by another node, wraps
    static uint32_t txCompletedCount();
    static bool txQueued();

    static TWAIState state();
    static uint8_t txErrorCounter();
    static uint8_t rxErrorCounter();
    // Returns and resets the counters
    static TWAIStats takeStats();
};

#endif

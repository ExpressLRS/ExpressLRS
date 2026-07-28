#pragma once
#include "handset.h"

#ifndef TARGET_NATIVE
#include "HardwareSerial.h"
#endif

#ifdef PLATFORM_ESP32
#include "driver/uart.h"
#endif

/**
 * @brief SBUS Handset implementation for receiving SBUS signal from external radio
 * 
 * This class receives SBUS protocol (100000 baud, inverted) on a configurable pin
 * and converts the 16 channels to CRSF format internally, making it appear as if
 * a native ELRS/Crossfire transmitter is connected.
 * 
 * SBUS Protocol:
 * - Baud rate: 100000
 * - Data bits: 8
 * - Parity: Even (emulated in software)
 * - Stop bits: 2
 * - Inverted logic
 * - Frame: 25 bytes (1 start byte + 22 data bytes + 1 end byte + optional 2 fail-safe bytes)
 */
class SBUSHandset final : public Handset
{
public:
    void Begin() override;
    void End() override;
    void handleInput() override;

private:
#if defined(PLATFORM_ESP32)
    static HardwareSerial SBUSport;
#endif
    
    uint8_t sbusBuffer[25];  // SBUS frame buffer
    uint8_t sbusFramePosition = 0;
    bool receivingFrame = false;
    uint32_t lastSbusFrame = 0;
    
    // SBUS constants
    static constexpr uint32_t SBUS_BAUD = 100000;
    static constexpr uint8_t SBUS_FRAME_SIZE = 25;
    static constexpr uint8_t SBUS_START_BYTE = 0x0F;
    static constexpr uint8_t SBUS_END_BYTE = 0x00;
    
    void processSbusFrame();
    uint16_t decodeChannel(uint8_t byte1, uint8_t byte2, uint8_t shift);
};

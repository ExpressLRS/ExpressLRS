#include "targets.h"

#if defined(PLATFORM_ESP32) && defined(TARGET_TX)

#include "SBUSHandset.h"
#include "OTA.h"
#include "crsf_protocol.h"
#include "logging.h"

#if defined(PLATFORM_ESP32)
// Use UART1 for SBUS input (UART0 is used by CRSFHandset)
HardwareSerial SBUSHandset::SBUSport(1);
#endif

void SBUSHandset::Begin()
{
    DBGLN("Starting SBUS Handset...");
    
    // Configure SBUS pin from hardware configuration
    // This allows the pin to be set via web interface through hardware.json
    int sbusPin = GPIO_PIN_RCSIGNAL_RX;
    
    if (sbusPin == UNDEF_PIN)
    {
        ERRLN("SBUS pin not configured! Set HARDWARE_serial1_rx in hardware.json");
        return;
    }
    
    DBGLN("SBUS configured on pin %d", sbusPin);
    
    // Initialize UART for SBUS
    // SBUS protocol: 100000 baud, 8E2 (8 data bits, Even parity, 2 stop bits), inverted
    // Note: ESP32 UART doesn't support even parity directly, so we use SERIAL_8N2 and handle it
    SBUSport.begin(SBUS_BAUD, SERIAL_8N2, sbusPin, -1, true); // RX pin, no TX, inverted=true
    SBUSport.setTimeout(0); // No timeout for continuous reading
    
    sbusFramePosition = 0;
    receivingFrame = false;
    
    // Clear any garbage in the buffer
    while (SBUSport.available())
    {
        SBUSport.read();
    }
    
    if (connected)
    {
        connected();
    }
}

void SBUSHandset::End()
{
    SBUSport.end();
    DBGLN("SBUS Handset stopped");
}

uint16_t SBUSHandset::decodeChannel(uint8_t byte1, uint8_t byte2, uint8_t shift)
{
    // SBUS channels are 11-bit values (0-2047)
    // Each channel spans across bytes in little-endian format
    uint16_t rawValue = ((uint16_t)byte2 << shift) | (byte1 >> (8 - shift));
    rawValue &= 0x07FF; // Mask to 11 bits (0-2047)
    
    // Convert SBUS range (172-1988) to CRSF range (172-1988)
    // SBUS: 172 = min, 1988 = max, 988 = center
    // CRSF: 172 = min, 1988 = max, 988 = center
    // They use the same range, so direct mapping works
    return rawValue;
}

void SBUSHandset::processSbusFrame()
{
    // Validate SBUS frame
    // Start byte should be 0x0F, end byte should be 0x00
    if (sbusBuffer[0] != SBUS_START_BYTE || sbusBuffer[24] != SBUS_END_BYTE)
    {
        DBGLN("Invalid SBUS frame: start=%02X end=%02X", sbusBuffer[0], sbusBuffer[24]);
        return;
    }
    
    uint32_t channels[CRSF_NUM_CHANNELS];
    const uint8_t MAX_SBUS_CHANNELS = 16;
    
    // Decode 16 channels from SBUS frame
    // Channel data starts at byte 1, each channel is 11 bits
    for (int i = 0; i < MAX_SBUS_CHANNELS; i++)
    {
        uint8_t byte1 = sbusBuffer[1 + i * 11 / 8];
        uint8_t byte2 = sbusBuffer[1 + i * 11 / 8 + 1];
        uint8_t shift = (i * 11) % 8;
        
        uint16_t sbusValue = decodeChannel(byte1, byte2, shift);
        
        // Map SBUS value to CRSF value
        // Both use 11-bit range (0-2047), but typical ranges are:
        // SBUS: 172 (min) to 1988 (max), 988 (center)
        // CRSF: 172 (min) to 1988 (max), 988 (center)
        // Direct mapping works as they use the same scale
        channels[i] = sbusValue;
    }
    
    // Extract flags from byte 23 (optional failsafe byte)
    // Bit 7: Channel 17 (not used here)
    // Bit 6: Channel 18 (not used here)
    // Bit 5: Frame lost
    // Bit 4: Fail-safe active
    bool failsafe = (sbusBuffer[23] & 0x10) != 0;
    bool frameLost = (sbusBuffer[23] & 0x08) != 0;
    
    if (failsafe || frameLost)
    {
        DBGLN("SBUS: Failsafe=%d FrameLost=%d", failsafe, frameLost);
        if (disconnected && controllerConnected)
        {
            disconnected();
        }
        controllerConnected = false;
        return;
    }
    
    // Perform any channel overrides before processing
    PerformChannelOverrides(channels, MAX_SBUS_CHANNELS);
    
    // Update armed state from channel 5 (AUX1 typically)
    // Same logic as PPMHandset
    if (MAX_SBUS_CHANNELS >= 5)
    {
        isArmed = CRSF_to_BIT(channels[4]);
    }
    
    // Notify that RC data has been received
    RCDataReceived(channels, MAX_SBUS_CHANNELS);
    
    if (!controllerConnected)
    {
        controllerConnected = true;
        DBGLN("SBUS connection established");
        if (connected)
        {
            connected();
        }
    }
    
    lastSbusFrame = millis();
}

void SBUSHandset::handleInput()
{
    const auto now = millis();
    
    // Read available bytes from UART
    while (SBUSport.available())
    {
        uint8_t byte = SBUSport.read();
        
        // Look for start byte
        if (!receivingFrame)
        {
            if (byte == SBUS_START_BYTE)
            {
                sbusBuffer[0] = byte;
                sbusFramePosition = 1;
                receivingFrame = true;
            }
            // Ignore other bytes until we find start byte
        }
        else
        {
            // Collect frame bytes
            sbusBuffer[sbusFramePosition++] = byte;
            
            // Check if frame is complete (25 bytes total)
            if (sbusFramePosition >= SBUS_FRAME_SIZE)
            {
                processSbusFrame();
                receivingFrame = false;
                sbusFramePosition = 0;
            }
        }
    }
    
    // Check for connection loss (no frames for 500ms)
    if (controllerConnected && lastSbusFrame > 0 && now - lastSbusFrame > 500)
    {
        DBGLN("SBUS signal lost");
        controllerConnected = false;
        isArmed = false;
        if (disconnected)
        {
            disconnected();
        }
    }
}

#endif

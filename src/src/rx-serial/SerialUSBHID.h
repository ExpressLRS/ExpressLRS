#pragma once

#include "common.h"
#include "SerialIO.h"

#if defined(TARGET_RX) && HAS_USB_HID_GAMEPAD

Stream *GetRxUsbSerialStream();
void RxUsbSerialBegin(uint32_t baud);
bool RxUsbSerialSupported();

class SerialUSBHID final : public SerialIO {
public:
    SerialUSBHID();
    ~SerialUSBHID() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;
    void sendQueuedData(uint32_t maxBytesToSend) override;
    void processSerialInput() override;

private:
    struct gamepad_report_t {
        int8_t x;
        int8_t y;
        int8_t z;
        int8_t rz;
        int8_t rx;
        int8_t ry;
        uint8_t hat;
        uint32_t buttons;
    };

    void processBytes(uint8_t *bytes, uint16_t size) override;

    static int8_t mapAxis(uint32_t value);
    static uint8_t mapHat(uint32_t horizontal, uint32_t vertical);

    gamepad_report_t lastReport {};
    bool reportInitialized = false;
};

#endif

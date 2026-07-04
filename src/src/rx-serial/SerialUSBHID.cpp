#include "SerialUSBHID.h"

#include "common.h"
#include "crsf_protocol.h"
#include "device.h"

#include <cstring>

#if defined(TARGET_RX) && HAS_USB_HID_GAMEPAD
#include "USB.h"
#include "USBCDC.h"
#include "USBHIDGamepad.h"

namespace {

USBCDC *rxUsbSerial = nullptr;
bool rxUsbStarted = false;
USBHIDGamepad *rxGamepad = nullptr;

constexpr char kUsbProductName[] = "ELRS USB Joystick";
constexpr char kUsbManufacturerName[] = "ELRS";

void configureUsbDeviceStrings()
{
    USB.productName(kUsbProductName);
    USB.manufacturerName(kUsbManufacturerName);
}

void ensureUsbStarted()
{
    if (!rxUsbStarted)
    {
        configureUsbDeviceStrings();
        USB.begin();
        rxUsbStarted = true;
    }
}

} // namespace

Stream *GetRxUsbSerialStream()
{
    return rxUsbSerial;
}

void RxUsbSerialBegin(uint32_t baud)
{
    if (rxUsbSerial == nullptr)
    {
        rxUsbSerial = new USBCDC(0);
    }

    rxUsbSerial->begin(baud);
    ensureUsbStarted();
}

bool RxUsbSerialSupported()
{
    return true;
}

SerialUSBHID::SerialUSBHID()
    : SerialIO(nullptr, nullptr)
{
    if (rxGamepad == nullptr)
    {
        rxGamepad = new USBHIDGamepad();
    }

    rxGamepad->begin();
}

int8_t SerialUSBHID::mapAxis(uint32_t value)
{
    const long clamped = constrain(static_cast<long>(value),
                                   static_cast<long>(CRSF_CHANNEL_VALUE_STD_MIN),
                                   static_cast<long>(CRSF_CHANNEL_VALUE_STD_MAX));
    return static_cast<int8_t>(map(clamped,
                                   static_cast<long>(CRSF_CHANNEL_VALUE_STD_MIN),
                                   static_cast<long>(CRSF_CHANNEL_VALUE_STD_MAX),
                                   -127L,
                                   127L));
}

uint8_t SerialUSBHID::mapHat(uint32_t horizontal, uint32_t vertical)
{
    constexpr int8_t threshold = 42;
    const int8_t x = mapAxis(horizontal);
    const int8_t y = mapAxis(vertical);

    const bool left = x <= -threshold;
    const bool right = x >= threshold;
    const bool up = y <= -threshold;
    const bool down = y >= threshold;

    if (up && right) return HAT_UP_RIGHT;
    if (down && right) return HAT_DOWN_RIGHT;
    if (down && left) return HAT_DOWN_LEFT;
    if (up && left) return HAT_UP_LEFT;
    if (up) return HAT_UP;
    if (right) return HAT_RIGHT;
    if (down) return HAT_DOWN;
    if (left) return HAT_LEFT;
    return HAT_CENTER;
}

uint32_t SerialUSBHID::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    (void)frameMissed;

    const bool neutralize = failsafe || !connectionHasModelMatch || !teamraceHasModelMatch;
    if (!neutralize && !frameAvailable)
    {
        return DURATION_IMMEDIATELY;
    }

    if (rxGamepad == nullptr)
    {
        return DURATION_IMMEDIATELY;
    }

    ensureUsbStarted();

    gamepad_report_t report {};
    if (!neutralize)
    {
        // Match the BLE joystick's axis naming as closely as standard HID allows:
        // BLE uses X, Y, Z, Rx, Ry, Rz, Slider1, Slider2.
        report.x = mapAxis(channelData[0]);
        report.y = mapAxis(channelData[1]);
        report.z = mapAxis(channelData[4]);
        report.rx = mapAxis(channelData[5]);
        report.ry = mapAxis(channelData[2]);
        report.rz = mapAxis(channelData[3]);
        report.hat = mapHat(channelData[6], channelData[7]);

        for (uint8_t i = 8; i < 16; ++i)
        {
            if (channelData[i] >= CRSF_CHANNEL_VALUE_2000)
            {
                report.buttons |= (1UL << (i - 8));
            }
        }
    }
    else
    {
        report.hat = HAT_CENTER;
    }

    if (!reportInitialized || std::memcmp(&report, &lastReport, sizeof(report)) != 0)
    {
        if (rxGamepad->send(report.x, report.y, report.z, report.rz, report.rx, report.ry, report.hat, report.buttons))
        {
            lastReport = report;
            reportInitialized = true;
        }
    }

    return DURATION_IMMEDIATELY;
}

void SerialUSBHID::sendQueuedData(uint32_t maxBytesToSend)
{
    (void)maxBytesToSend;
}

void SerialUSBHID::processSerialInput()
{
}

void SerialUSBHID::processBytes(uint8_t *bytes, uint16_t size)
{
    (void)bytes;
    (void)size;
}

#endif

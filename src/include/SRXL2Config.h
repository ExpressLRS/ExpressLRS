#pragma once

#include "common.h"

#if defined(PLATFORM_ESP32)
#include "soc/soc_caps.h"
#endif

inline bool isSRXL2Selected(uint8_t port, uint8_t primaryProtocol, uint8_t secondaryProtocol, bool isAirport)
{
    if (port == 0) return primaryProtocol == PROTOCOL_SRXL2 && !isAirport;
#if defined(PLATFORM_ESP32) || defined(SRXL2_ADAPTER_TEST)
    if (port == 1) return secondaryProtocol == PROTOCOL_SERIAL1_SRXL2;
#endif
    return false;
}

inline bool supportsSRXL2(uint8_t port, int8_t signalPin, bool primaryEnabled)
{
#if defined(TARGET_RX) && defined(PLATFORM_ESP32) && \
    (defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3))
    return port <= 1 && signalPin >= 0 && signalPin < 64 &&
        ((uint64_t(SOC_GPIO_VALID_OUTPUT_GPIO_MASK) >> signalPin) & 1) &&
        (port != 0 || primaryEnabled);
#else
    return false;
#endif
}

inline bool supportsSRXL2(uint8_t port, int8_t signalPin)
{
#if defined(TARGET_RX)
    return supportsSRXL2(port, signalPin, !OPT_CRSF_RCVR_NO_SERIAL);
#else
    return false;
#endif
}

// Arduino infers UART0's default pins only when both fixed fields are absent.
#if defined(TARGET_RX)
inline void resolvePrimarySerialPins(int8_t &rx, int8_t &tx, bool enabled)
{
    rx = GPIO_PIN_RCSIGNAL_RX;
    tx = GPIO_PIN_RCSIGNAL_TX;
    if (enabled && rx == UNDEF_PIN && tx == UNDEF_PIN)
    {
        rx = U0RXD_GPIO_NUM;
        tx = U0TXD_GPIO_NUM;
    }
}

template<typename ModeAt>
inline bool primarySerialEnabled(ModeAt modeAt)
{
#if defined(DEBUG_CRSF_NO_OUTPUT)
    return false;
#else
    if (GPIO_PIN_RCSIGNAL_RX != UNDEF_PIN || GPIO_PIN_RCSIGNAL_TX != UNDEF_PIN) return true;
    if (OPT_HAS_SERVO_OUTPUT)
        for (uint8_t ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
            if (modeAt(ch) == somSerial) return true;
    return false;
#endif
}

#if defined(PLATFORM_ESP32)
template<typename ModeAt>
inline void resolveSerial1Pins(int8_t &rx, int8_t &tx, ModeAt modeAt)
{
    rx = GPIO_PIN_SERIAL1_RX;
    tx = GPIO_PIN_SERIAL1_TX;
    if (rx == UNDEF_PIN)
        for (uint8_t ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
            if (modeAt(ch) == somSerial1RX) rx = GPIO_PIN_PWM_OUTPUTS[ch];
    if (tx == UNDEF_PIN)
        for (uint8_t ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
            if (modeAt(ch) == somSerial1TX) tx = GPIO_PIN_PWM_OUTPUTS[ch];
}
#endif
#endif

inline bool supportsSRXL2()
{
#if defined(TARGET_RX) && defined(PLATFORM_ESP32)
    int8_t rx, tx;
    resolvePrimarySerialPins(rx, tx, !OPT_CRSF_RCVR_NO_SERIAL);
    return supportsSRXL2(0, tx);
#else
    return false;
#endif
}

inline bool isValidSerialProtocolPair(uint8_t primary, uint8_t secondary)
{
#if defined(PLATFORM_ESP32)
    return primary <= PROTOCOL_SRXL2 && secondary <= PROTOCOL_SERIAL1_SRXL2 &&
        !(primary == PROTOCOL_SRXL2 && secondary == PROTOCOL_SERIAL1_SRXL2);
#else
    return primary <= PROTOCOL_SRXL2 && secondary == 0;
#endif
}

#if defined(TARGET_RX)
template<typename ModeAt>
inline bool isValidSRXL2Pin(uint8_t port, uint8_t primary, uint8_t secondary, bool isAirport, ModeAt modeAt)
{
    const bool primaryEnabled = primarySerialEnabled(modeAt);
    int8_t rx, tx;
    resolvePrimarySerialPins(rx, tx, primaryEnabled);
    int8_t otherRx = UNDEF_PIN, otherTx = UNDEF_PIN;
    uint8_t requiredMode = somSerial;
#if defined(PLATFORM_ESP32)
    if (port == 1)
    {
        if (primaryEnabled)
        {
            otherRx = isSRXL2Selected(0, primary, secondary, isAirport) ? UNDEF_PIN : rx;
            otherTx = tx;
        }
        resolveSerial1Pins(rx, tx, modeAt);
        requiredMode = somSerial1TX;
    }
    else if (secondary != PROTOCOL_SERIAL1_OFF && secondary != PROTOCOL_SERIAL1_SRXL2)
    {
        // Primary Smart wins at boot over a malformed stored both-Smart pair.
        resolveSerial1Pins(otherRx, otherTx, modeAt);
        if (secondary == PROTOCOL_SERIAL1_GPS && otherRx == UNDEF_PIN)
            otherTx = UNDEF_PIN; // The stock GPS factory leaves this port inactive.
        if (secondary == PROTOCOL_SERIAL1_SBUS || secondary == PROTOCOL_SERIAL1_INVERTED_SBUS ||
            secondary == PROTOCOL_SERIAL1_DJI_RS_PRO || secondary == PROTOCOL_SERIAL1_SUMD ||
            secondary == PROTOCOL_SERIAL1_TRAMP || secondary == PROTOCOL_SERIAL1_SMARTAUDIO ||
            secondary == PROTOCOL_SERIAL1_MSP_DISPLAYPORT)
            otherRx = UNDEF_PIN; // These stock cases initialize TX only.
        if (secondary != PROTOCOL_SERIAL1_GPS && otherRx < 0 && otherTx < 0)
        {
            otherRx = int8_t(RX1); // HardwareSerial.begin infers both pins, even in TX-only cases.
            otherTx = int8_t(TX1);
        }
    }
#endif
    if (!supportsSRXL2(port, tx, primaryEnabled) || tx == otherRx || tx == otherTx) return false;
    for (uint8_t ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ++ch)
        if (GPIO_PIN_PWM_OUTPUTS[ch] == tx && modeAt(ch) != requiredMode) return false;
    return true;
}

template<typename ModeAt>
inline bool isValidSRXL2Config(uint8_t primary, uint8_t secondary, bool isAirport, ModeAt modeAt)
{
    if (!isValidSerialProtocolPair(primary, secondary)) return false;
    if (primary == PROTOCOL_SRXL2 && !isValidSRXL2Pin(0, primary, secondary, isAirport, modeAt)) return false;
#if defined(PLATFORM_ESP32)
    if (secondary == PROTOCOL_SERIAL1_SRXL2 && !isValidSRXL2Pin(1, primary, secondary, isAirport, modeAt)) return false;
#endif
    return true;
}

// Preview configureSerialPin's stock GPIO1/3 sibling effect; never change config here.
template<typename ModeAt>
inline bool isValidSRXL2PwmChange(uint8_t ch, uint8_t mode, uint8_t primary, uint8_t secondary, bool isAirport, ModeAt modeAt)
{
    const int8_t pin = GPIO_PIN_PWM_OUTPUTS[ch];
    int8_t siblingCh = -1;
    for (uint8_t candidate = 0; candidate < GPIO_PIN_PWM_OUTPUTS_COUNT; ++candidate)
        if ((pin == 1 && GPIO_PIN_PWM_OUTPUTS[candidate] == 3) || (pin == 3 && GPIO_PIN_PWM_OUTPUTS[candidate] == 1))
        {
            siblingCh = candidate;
            break;
        }
    return isValidSRXL2Config(primary, secondary, isAirport, [=](uint8_t candidate) -> uint8_t {
        if (candidate == ch) return mode;
        const uint8_t oldMode = modeAt(candidate);
        if (candidate != siblingCh) return oldMode;
        return mode == somSerial ? somSerial : oldMode == somSerial ? som50Hz : oldMode;
    });
}
#endif

#pragma once

#include <cstdint>
#include <array>

#if defined(WMEXTENSION)

struct MultiSwitch {
    MultiSwitch();
    using switches_t = std::array<uint8_t, 8>;
    using ledColor_t = std::array<uint32_t, 64>;

    void decode(const uint8_t*);

    const switches_t& switches() const;
    bool state(uint8_t) const;
    bool ledState(uint8_t) const;
    uint32_t ledColor(uint8_t) const;
    bool hasData() const;
    uint8_t channelFlags() const;

    private:    
    bool mHasData = false;
    const uint8_t minAddress = 240;
    const uint8_t maxAddress = minAddress + 8 - 1;
    const uint8_t minLedAddress = maxAddress + 1;
    const uint8_t maxLedAddress = minLedAddress + 8 - 1;
    switches_t mSwitches{};
    switches_t mLeds{};
    ledColor_t mLedColors{};
    uint8_t mFlags{};
};

#endif

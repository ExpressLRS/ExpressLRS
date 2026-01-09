#if defined(WMEXTENSION)

#include "rx_wmextension.h"

#include "targets.h"
#include "FIFO.h"
#include "device.h"

#include "common.h"
#include "crsf_protocol.h"

MultiSwitch::MultiSwitch() {
    for(auto& c : mLedColors) {
        c = (0 << 16) + (32 << 8) + (0 << 0);
    }
}
uint8_t MultiSwitch::channelFlags() const {
    return mFlags;
}
bool MultiSwitch::hasData() const {
    return mHasData;
}
const MultiSwitch::switches_t& MultiSwitch::switches() const {
    return mSwitches;
}
bool MultiSwitch::state(const uint8_t sw) const {
    if (sw < mLedColors.size()) {
        const uint8_t swidx = sw / 8;
        const uint8_t swbit = sw % 8;
        return (mSwitches[swidx] & (1 << swbit));
    }
    return false;
}
bool MultiSwitch::ledState(const uint8_t led) const {
    if (led < mLedColors.size()) {
        const uint8_t lidx = led / 8;
        const uint8_t lbit = led % 8;
        return (mLeds[lidx] & (1 << lbit));
    }
    return false;
}
uint32_t MultiSwitch::ledColor(const uint8_t led) const {
    if (led < mLedColors.size()) {
        return mLedColors[led];
    }
    return 0;
}
void MultiSwitch::decode(const uint8_t* const data) {
    DBGLN("MSW decode");
    const uint8_t realm = data[0]; // 5
    const uint8_t cmd = data[1]; // 6
    if (realm == CRSF_COMMAND_SWITCH) { 
        DBGLN("MSW Realm SW: cmd: %d", cmd);
        if (cmd == CRSF_SUBCMD_SWITCH_SET) { 
            const uint8_t swAddress = data[2];
            const uint16_t sw = data[3];
            if ((swAddress >= minAddress) && (swAddress <= maxAddress)) {
                mHasData = true;
                const uint8_t group = (swAddress - minAddress) & 0x07;
                for(uint8_t i = 0; i < 8; ++i) {
                    const bool on = (((sw >> i) & 0b01) > 0);
                    const uint8_t mask = (1 << i);
                    if (on) {
                        mSwitches[group] |= mask;
                    }
                    else {
                        mSwitches[group] &= ~mask;
                    }
                }
            }
            else if ((swAddress >= minLedAddress) && (swAddress <= maxLedAddress)) {
                mHasData = true;
                const uint8_t group = (swAddress - minLedAddress) & 0x07;
                for(uint8_t i = 0; i < 8; ++i) {
                    const bool on = (((sw >> i) & 0b01) > 0);
                    const uint8_t mask = (1 << i);
                    if (on) {
                        mLeds[group] |= mask;
                    }
                    else {
                        mLeds[group] &= ~mask;
                    }
                }
            }
        }
        else if (cmd == CRSF_SUBCMD_SWITCH_SET4) {
            const uint8_t swAddress = data[2];
            const uint16_t sw = (data[3] << 8) + data[4];
            DBGLN("MSW Set4: adr: %d, v: %d", swAddress, sw);
            if ((swAddress >= minAddress) && (swAddress <= maxAddress)) {
                mHasData = true;
                const uint8_t group = (swAddress - minAddress) & 0x07;
                for(uint8_t i = 0; i < 8; ++i) {
                    const bool on = (((sw >> (2 * i)) & 0b11) > 0);
                    const uint8_t mask = (1 << i);
                    if (on) {
                        DBGLN("MSW Set4 sw on:", i);
                        mSwitches[group] |= mask;
                    }
                    else {
                        DBGLN("MSW Set4 sw off:", i);
                        mSwitches[group] &= ~mask;
                    }
                }
            }
            else if ((swAddress >= minLedAddress) && (swAddress <= maxLedAddress)) {
                mHasData = true;
                const uint8_t group = (swAddress - minLedAddress) & 0x07;
                for(uint8_t i = 0; i < 8; ++i) {
                    const bool on = (((sw >> (2 * i)) & 0b11) > 0);
                    const uint8_t mask = (1 << i);
                    if (on) {
                        DBGLN("MSW Set4 led on:", i);
                        mLeds[group] |= mask;
                    }
                    else {
                        DBGLN("MSW Set4 led off:", i);
                        mLeds[group] &= ~mask;
                    }
                }
            }
        }
        else if (cmd == CRSF_SUBCMD_SWITCH_SET4M) { 
            const uint8_t count = data[2];
            for(uint8_t i = 0; i < count; ++i) {
                const uint8_t swAddress = data[3 + 3 * i];
                const uint16_t sw = (data[4 + 3 * i] << 8) + data[5 + 3 * i];
                DBGLN("MSW Set4M: cnt: %d, adr: %d, v: %d", count, swAddress, sw);
                if ((swAddress >= minAddress) && (swAddress <= maxAddress)) {
                    mHasData = true;
                    const uint8_t swGroup = (swAddress - minAddress) & 0x07;
                    for(uint8_t k = 0; k < 8; ++k) {
                        const bool on = (((sw >> (2 * k)) & 0b11) > 0);
                        const uint8_t mask = (1 << k);
                        if (on) {
                            mSwitches[swGroup] |= mask;
                        }
                        else {
                            mSwitches[swGroup] &= ~mask;
                        }
                    }
                }
                else if ((swAddress >= minLedAddress) && (swAddress <= maxLedAddress)) {
                    mHasData = true;
                    const uint8_t swGroup = (swAddress - minLedAddress) & 0x07;
                    for(uint8_t k = 0; k < 8; ++k) {
                        const bool on = (((sw >> (2 * k)) & 0b11) > 0);
                        const uint8_t mask = (1 << k);
                        if (on) {
                            mLeds[swGroup] |= mask;
                        }
                        else {
                            mLeds[swGroup] &= ~mask;
                        }
                    }
                }
            }
        }
        else if (cmd == CRSF_SUBCMD_SWITCH_SET64) { // set64
            const uint8_t swAddress = data[2];
            const uint8_t swGroup = (data[3] & 0x07);
            const uint16_t swSwitches = (data[4] << 8) + data[5];
            DBGLN("MSW Set64: adr: %d, grp: %d, v: %d", swAddress, swGroup, swSwitches);
            if (swAddress == minAddress) {
                mHasData = true;
                for(uint8_t i = 0; i < 8; ++i) {
                    const bool on = (((swSwitches >> (2 * i)) & 0b11) > 0);
                    const uint8_t mask = (1 << i);
                    if (on) {
                        mSwitches[swGroup] |= mask;
                    }
                    else {
                        mSwitches[swGroup] &= ~mask;
                    }
                }
            }
        }
        else if (cmd == CRSF_SUBCMD_SWITCH_SETRGB) { // setRGB
            const uint8_t swAddress = data[2];
            const uint8_t count = data[3];
            if ((swAddress >= minLedAddress) && (swAddress <= maxLedAddress) && (count <= 8)) {
                mHasData = true;
                const uint8_t swGroup = (swAddress - minLedAddress) & 0x07;;
                for(uint8_t i = 0; i < count; ++i) {
                    const uint8_t out = (data[4 + 2 * i] >> 4) & 0x07;
                    const uint32_t r = ((data[4 + 2 * i]) & 0x0f) << 4;
                    const uint32_t g = ((data[5 + 2 * i] >> 4) & 0x0f) << 4;
                    const uint32_t b = ((data[5 + 2 * i]) & 0x0f) << 4;
                    const uint32_t color = (r << 16) + (g << 8) + b;
                    DBGLN("MSW SetRGB: out: %d, r: %d, g: %d, b: %d", out, r, g, b);
                    mLedColors[swGroup * 8 + out] = color;
                }
            }
        }
    }
    else if (realm == CRSF_COMMAND_CC) { // cruise controller
        DBGLN("SUMDV3 Realm CC: cmd: %d", cmd);
        if (cmd == CRSF_SUBCMD_CC_SETCHANNEL) { // 16 channels as 8-bit
            mFlags = 0;
            for(uint8_t i = 0; i < CRSF_EXTRA_CHANNELS; ++i) {
                const int8_t ch8bit = data[2 + i];
                ChannelData[i + CRSF_NUM_CHANNELS] = (int32_t(ch8bit) * ((CRSF_CHANNEL_VALUE_MAX - CRSF_CHANNEL_VALUE_MIN) / 2)) / 127 + CRSF_CHANNEL_VALUE_MID; 
            }
        }
        else if (cmd == CRSF_SUBCMD_CC_SETCHANNEL_EXT) { // flags, 16 channels as 8-bit
            mFlags = data[2];
            for(uint8_t i = 0; i < CRSF_EXTRA_CHANNELS; ++i) {
                const int8_t ch8bit = data[3 + i];
                ChannelData[i + CRSF_NUM_CHANNELS] = (int32_t(ch8bit) * ((CRSF_CHANNEL_VALUE_MAX - CRSF_CHANNEL_VALUE_MIN) / 2)) / 127 + CRSF_CHANNEL_VALUE_MID; 
            }
        }
    }
} 
#endif
#pragma once

void startWaveform8266(uint8_t pin, uint32_t timeHighUS, uint32_t timeLowUS);
void stopWaveform8266(uint8_t pin);

// This section below is a hack to prevent the use of the core's core_esp8266_waveform functions by
// overriding their implementations. It seems to work and save about 800 bytes of RAM and 500 bytes of code
extern "C"
{
    int startWaveform(uint8_t pin, uint32_t timeHighUS, uint32_t timeLowUS, uint32_t runTimeUS,
                    int8_t alignPhase, uint32_t phaseOffsetUS, bool autoPwm) { return false; };
    int startWaveformClockCycles(uint8_t pin, uint32_t timeHighCycles, uint32_t timeLowCycles, uint32_t runTimeCycles,
                                int8_t alignPhase, uint32_t phaseOffsetUS, bool autoPwm) { return false; };
    void stopWaveform(uint8_t pin) {};
    void setTimer1Callback(uint32_t (*fn)()) {};
    void _setPWMFreq(uint32_t freq) {};
    void _stopPWM(uint8_t pin) {};
    bool _setPWM(int pin, uint32_t val, uint32_t range) { return false; };
}
// Simulated hardware underneath the real rx_main.cpp, see sim_hardware.h
#include "sim_hardware.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include <sys/types.h>

#include "targets.h"
#include "common.h"
#include "device.h"
#include "devServoOutput.h"
#include "elrs_eeprom.h"
#include "hwTimer.h"
#include "native.h"
#include "options.h"
#include "SerialHoTT_TLM.h"
#include "SerialIO.h"
#include "SerialMavlink.h"
#include "SX1280.h"
#include "SX1280_Regs.h"
#include "SX12xxDriverCommon.h"

#include "arduino_stubs.h"

FuzzFS LittleFS;
FuzzESP ESP;
FuzzWire Wire;

SimRadio simRadio;
std::vector<uint8_t> simSerialOut;

static uint64_t now;
static uint64_t timerNext;
static uint64_t radioTxDoneAt;
static bool inEvent;
static unsigned microsCallsSinceAdvance;

static void runTimerEvent();
// hwTimer::callback is private, init() leaves a pointer to it here
static void (*timerCallback)();

// micros() for the firmware
static unsigned long simMicros()
{
    // rx_main busy-waits on micros() for the next tock, so a clock that only moves between events would hang it
    if (!inEvent && ++microsCallsSinceAdvance > 1000)
    {
        if (hwTimer::running)
        {
            now = timerNext;
            runTimerEvent();
        }
        else
        {
            now += 100;
        }
        nativeClockMs() = now / 1000;
    }
    return (uint32_t)now;
}

// Collects what the firmware writes to Serial
static void serialSink(const uint8_t *data, size_t len)
{
    simSerialOut.insert(simSerialOut.end(), data, data + len);
}

// Points the firmware's clock and Serial at the simulation
void simInstall()
{
    // Not zero: the firmware treats a zero timestamp as "never"
    now = 1000000;
    nativeClockMs() = now / 1000;
    nativeMicrosSource() = simMicros;
    nativeSerialSink() = serialSink;
}

// Simulated time in microseconds
uint64_t simNow()
{
    return now;
}

// Runs the timer and radio events that are due up to time t
void simAdvanceTo(uint64_t t, void (*afterEvent)())
{
    for (;;)
    {
        const bool timerDue = hwTimer::running && timerNext <= t;
        const bool txDoneDue = radioTxDoneAt && radioTxDoneAt <= t;
        if (!timerDue && !txDoneDue)
            break;

        microsCallsSinceAdvance = 0;
        if (txDoneDue && (!timerDue || radioTxDoneAt <= timerNext))
        {
            now = radioTxDoneAt;
            radioTxDoneAt = 0;
            nativeClockMs() = now / 1000;
            inEvent = true;
            Radio.TXdoneCallback();
            inEvent = false;
        }
        else
        {
            now = timerNext;
            nativeClockMs() = now / 1000;
            runTimerEvent();
        }
        afterEvent();
    }
    if (t > now)
        now = t;
    nativeClockMs() = now / 1000;
    microsCallsSinceAdvance = 0;
}

// hwTimer, same arithmetic as ESP8266_hwTimer.cpp with one tick per microsecond

void (*hwTimer::callbackTick)() = nullptr;
void (*hwTimer::callbackTock)() = nullptr;
volatile bool hwTimer::running = false;
volatile bool hwTimer::isTick = false;
volatile uint32_t hwTimer::HWtimerInterval = TimerIntervalUSDefault;
volatile int32_t hwTimer::PhaseShift = 0;
volatile int32_t hwTimer::FreqOffset = 0;

// Stores the tick and tock callbacks
void hwTimer::init(void (*callbackTick)(), void (*callbackTock)())
{
    hwTimer::callbackTick = callbackTick;
    hwTimer::callbackTock = callbackTock;
    timerCallback = hwTimer::callback;
    running = false;
}

// Stops the timer
void hwTimer::stop()
{
    running = false;
}

// Starts the timer, first tock in 20us
void hwTimer::resume()
{
    if (!running)
    {
        isTick = false;
        timerNext = now + 20;
        running = true;
    }
}

// Sets the tick-tock period in microseconds
void hwTimer::updateInterval(uint32_t newTimerInterval)
{
    HWtimerInterval = newTimerInterval;
}

// One-off delay applied after the next tock
void hwTimer::phaseShift(int32_t newPhaseShift)
{
    int32_t minVal = -(HWtimerInterval >> 2);
    int32_t maxVal = (HWtimerInterval >> 2);
    PhaseShift = constrain(newPhaseShift, minVal, maxVal);
}

// Fires tick or tock and schedules the next one
void hwTimer::callback()
{
    timerNext += (HWtimerInterval >> 1) + FreqOffset;
    if (isTick)
    {
        callbackTick();
    }
    else
    {
        timerNext += PhaseShift;
        PhaseShift = 0;
        callbackTock();
    }
    isTick = !isTick;
}

// Runs the timer callback as an interrupt would
static void runTimerEvent()
{
    inEvent = true;
    timerCallback();
    inEvent = false;
}

// Radio. Replaces SX1280.cpp: no chip is emulated, packets are handed over by simRadioReceive().

SX1280Driver *SX1280Driver::instance = nullptr;

// The simulated radio never reports a frequency error
SX1280Driver::SX1280Driver() : modeSupportsFei(false)
{
    instance = this;
}

// Radio init always succeeds
bool SX1280Driver::Begin()
{
    simRadio.listening = false;
    return true;
}

// Stops listening and drops the callbacks
void SX1280Driver::End()
{
    simRadio.listening = false;
    RemoveCallbacks();
}

// Records the modulation settings the RX asked for
void SX1280Driver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t PreambleLength, bool InvertIQ,
                          uint8_t _PayloadLength, uint32_t flrcSyncWord, uint16_t flrcCrcSeed, RadioBandMod::Combined modulation)
{
    PayloadLength = _PayloadLength;
    IQinverted = InvertIQ;
    currFreq = freq;
    simRadio.listening = false;
    simRadio.freq = freq;
    simRadio.bw = bw;
    simRadio.sf = sf;
    simRadio.cr = cr;
    simRadio.payloadLength = _PayloadLength;
}

// Records the frequency the RX tuned to
void SX1280Driver::SetFrequencyReg(uint32_t freq, SX12XX_Radio_Number_t radioNumber, bool doRx)
{
    currFreq = freq;
    simRadio.freq = freq;
    if (doRx)
        simRadio.listening = true;
}

void SX1280Driver::SetOutputPower(int8_t power) {}

// Any mode set this way is not receive
void SX1280Driver::SetMode(SX1280_RadioOperatingModes_t OPmode, SX12XX_Radio_Number_t radioNumber)
{
    simRadio.listening = false;
}

// No frequency error to report
bool SX1280Driver::GetFrequencyErrorbool(SX12XX_Radio_Number_t radioNumber)
{
    return false;
}

// Fixed, quiet channel
int8_t SX1280Driver::GetRssiInst(SX12XX_Radio_Number_t radioNumber)
{
    return -100;
}

// There is no second radio
void SX1280Driver::CheckForSecondPacket()
{
    hasSecondRadioGotData = false;
}

// Fixed, strong signal
void SX1280Driver::GetLastPacketStats()
{
    LastPacketRSSI = -50;
    LastPacketRSSI2 = -50;
    LastPacketSNRRaw = 10 * RADIO_SNR_SCALE;
}

// Starts listening
void SX1280Driver::RXnb()
{
    simRadio.listening = true;
}

// Telemetry: counts the packet and schedules its TX-done event
void SX1280Driver::TXnb(uint8_t *data, bool sendGeminiBuffer, uint8_t *dataGemini, SX12XX_Radio_Number_t radioNumber)
{
    simRadio.listening = false;
    transmittingRadio = radioNumber;
    if (radioNumber == SX12XX_Radio_NONE)
        return;
    simRadio.packetsSent++;
    radioTxDoneAt = now + ExpressLRS_currAirRate_RFperfParams->TOA;
}

// Hands a packet to the firmware's RX-done handler
bool simRadioReceive(const uint8_t *data, size_t len)
{
    memcpy(Radio.RXdataBuffer, data, len);
    Radio.processingPacketRadio = SX12XX_Radio_1;
    Radio.strongestReceivingRadio = SX12XX_Radio_1;
    microsCallsSinceAdvance = 0;
    inEvent = true;
    const bool accepted = Radio.RXdoneCallback(SX12xxDriverCommon::SX12XX_RX_OK);
    inEvent = false;
    return accepted;
}

// EEPROM in RAM. Starts blank, so the first Load() falls back to defaults.

static uint8_t eepromData[RESERVED_EEPROM_SIZE];
void ELRS_EEPROM::Begin() {}
void ELRS_EEPROM::Commit() {}

// Reads the RAM EEPROM
uint8_t ELRS_EEPROM::ReadByte(const uint32_t address)
{
    return address < sizeof(eepromData) ? eepromData[address] : 0;
}

// Writes the RAM EEPROM
void ELRS_EEPROM::WriteByte(const uint32_t address, const uint8_t value)
{
    if (address < sizeof(eepromData))
        eepromData[address] = value;
}

// Build options and hardware layout: a plain serial RX, no PWM, no optional hardware

firmware_options_t firmwareOptions;
char device_name[] = "fuzz";
char product_name[] = "fuzz";
const unsigned char target_name[] = "\xBE\xEF\xCA\xFE" "FUZZ";
const uint8_t target_name_size = sizeof(target_name);
const char commit[] = "fuzz";
const char version[] = "fuzz";
bool webserverPreventAutoStart;

// Build options of a plain serial RX
bool options_init()
{
    firmwareOptions.uart_baud = 420000;
    firmwareOptions.wifi_auto_on_interval = -1;
    firmwareOptions.lock_on_first_connection = true;
    return true;
}

void options_SetTrueDefaults() {}

// Only the serial pins exist
int hardware_pin(nameType name)
{
    switch (name)
    {
        case HARDWARE_serial_rx: return 3;
        case HARDWARE_serial_tx: return 1;
        default: return UNDEF_PIN;
    }
}

// No optional hardware
bool hardware_flag(nameType name)
{
    return false;
}

// No PWM outputs
int hardware_int(nameType name)
{
    return 0;
}

// No calibration values
float hardware_float(nameType name)
{
    return 0;
}

// No tables
const int16_t *hardware_i16_array(nameType name)
{
    return nullptr;
}

// No tables
const uint16_t *hardware_u16_array(nameType name)
{
    return nullptr;
}

// Devices with nothing behind them here

static device_t nullDevice = {nullptr, nullptr, nullptr, nullptr, 0};
device_t AnalogVbat_device = nullDevice;
device_t Baro_device = nullDevice;
device_t RGB_device = nullDevice;
device_t ServoOut_device = nullDevice;
device_t WIFI_device = nullDevice;
void Vbat_enableSlowUpdate(bool enable) {}
void servoNewChannelsAvailable() {}
void servoCurrentToFailsafeConfig() {}
void setWifiUpdateMode() {}

// MAVLink and HoTT need libraries that are not in the tree. They are never selected here.

SerialMavlink::SerialMavlink(Stream &out, Stream &in) : SerialIO(&out, &in), this_component_id(0), target_component_id(0) {}

// Never sends
uint32_t SerialMavlink::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    return DURATION_NEVER;
}

// One byte, as zero makes SerialIO declare an empty array
int SerialMavlink::getMaxSerialReadSize()
{
    return 1;
}

void SerialMavlink::sendQueuedData(uint32_t maxBytesToSend) {}
void SerialMavlink::forwardMessage(const uint8_t *data) {}

// Never has telemetry to send
bool SerialMavlink::GetNextPayload(uint8_t *nextPayloadSize, uint8_t *payloadData)
{
    return false;
}

void SerialMavlink::event() {}
void SerialMavlink::processBytes(uint8_t *bytes, u_int16_t size) {}

SerialHoTT_TLM::SerialHoTT_TLM(Stream &out, Stream &in, int8_t serial1TXpin) : SerialIO(&out, &in) {}

// One byte, as zero makes SerialIO declare an empty array
int SerialHoTT_TLM::getMaxSerialReadSize()
{
    return 1;
}

void SerialHoTT_TLM::sendQueuedData(uint32_t maxBytesToSend) {}
void SerialHoTT_TLM::processBytes(uint8_t *bytes, u_int16_t size) {}

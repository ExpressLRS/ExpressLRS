#include "rxtx_common.h"
#include "LowPassFilter.h"

#include "crc.h"
#include "telemetry_protocol.h"
#include "telemetry.h"
#include "stubborn_sender.h"
#include "stubborn_receiver.h"

#include "lua.h"
#include "msp.h"
#include "msptypes.h"
#include "PFD.h"
#include "options.h"
#include "dynpower.h"
#include "MeanAccumulator.h"
#include "freqTable.h"

#include "rx-serial/SerialIO.h"
#include "rx-serial/SerialNOOP.h"
#include "rx-serial/SerialCRSF.h"
#include "rx-serial/SerialSBUS.h"
#include "rx-serial/SerialSUMD.h"
#include "rx-serial/SerialAirPort.h"
#include "rx-serial/SerialHoTT_TLM.h"
#include "rx-serial/SerialMavlink.h"
#include "rx-serial/SerialTramp.h"
#include "rx-serial/SerialSmartAudio.h"
#include "rx-serial/SerialDisplayport.h"

#include "rx-serial/devSerialIO.h"
#include "devLED.h"
#include "devLUA.h"
#include "devWIFI.h"
#include "devButton.h"
#include "devServoOutput.h"
#include "devVTXSPI.h"
#include "devAnalogVbat.h"
#include "devSerialUpdate.h"
#include "devBaro.h"
#include "devMSPVTX.h"
#include "devThermal.h"

#if defined(PLATFORM_ESP8266)
#include <user_interface.h>
#include <FS.h>
#elif defined(PLATFORM_ESP32)
#include <SPIFFS.h>
#include "esp_task_wdt.h"
#endif

//
// Code encapsulated by the ARDUINO_CORE_INVERT_FIX #ifdef temporarily fixes EpressLRS issue #2609 which is caused
// by the Arduino core (see https://github.com/espressif/arduino-esp32/issues/9896) and fixed
// by Espressif with Arduino core release 3.0.3 (see https://github.com/espressif/arduino-esp32/pull/9950)
//
// With availability of Arduino core 3.0.3 and upgrading ExpressLRS to Arduino core 3.0.3 the temporary fix
// should be deleted again
//
// ARDUINO_CORE_INVERT_FIX PT1
#define ARDUINO_CORE_INVERT_FIX

#if defined(PLATFORM_ESP32) && defined(ARDUINO_CORE_INVERT_FIX)
#include "driver/uart.h"
#endif
// ARDUINO_CORE_INVERT_FIX PT1 end

///LUA///
#define LUA_MAX_PARAMS 32
////

//// CONSTANTS ////
#define SEND_LINK_STATS_TO_FC_INTERVAL 100
#define DIVERSITY_ANTENNA_INTERVAL 5
#define DIVERSITY_ANTENNA_RSSI_TRIGGER 5
#define PACKET_TO_TOCK_SLACK 200 // Desired buffer time between Packet ISR and Tock ISR
///////////////////

device_affinity_t ui_devices[] = {
  {&Serial0_device, 1},
#if defined(PLATFORM_ESP32)
  {&Serial1_device, 1},
  {&SerialUpdate_device, 1},
#endif
  {&LED_device, 0},
  {&LUA_device, 0},
  {&RGB_device, 0},
  {&WIFI_device, 0},
  {&Button_device, 0},
#ifdef USE_ANALOG_VBAT
  {&AnalogVbat_device, 0},
#endif
#ifdef HAS_SERVO_OUTPUT
  {&ServoOut_device, 1},
#endif
#if defined(PLATFORM_ESP32)
#ifdef HAS_BARO
  {&Baro_device, 0}, // must come after AnalogVbat_device to slow updates
#endif
#ifdef HAS_VTX_SPI
  {&VTxSPI_device, 0},
#endif
#ifdef HAS_MSP_VTX
  {&MSPVTx_device, 0}, // dependency on VTxSPI_device
#endif
  {&Thermal_device, 0},
#endif
};

uint8_t antenna = 0;    // which antenna is currently in use
uint8_t geminiMode = 0;

PFD PFDloop;
Crc2Byte ota_crc;
ELRS_EEPROM eeprom;
RxConfig config;
Telemetry telemetry;
Stream *SerialLogger;

#if defined(USE_MSP_WIFI)
#include "crsf2msp.h"
#include "msp2crsf.h"

CROSSFIRE2MSP crsf2msp;
MSP2CROSSFIRE msp2crsf;
#endif

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
unsigned long rebootTime = 0;
extern bool webserverPreventAutoStart;
bool pwmSerialDefined = false;
#endif
uint32_t serialBaud;

/* SERIAL_PROTOCOL_TX is used by CRSF output */
#define SERIAL_PROTOCOL_TX Serial

#if defined(PLATFORM_ESP32)
    #define SERIAL1_PROTOCOL_TX Serial1

    // SBUS driver needs to distinguish stream for SBUS/DJI protocol
    const Stream *serial_protocol_tx = &(SERIAL_PROTOCOL_TX);
    const Stream *serial1_protocol_tx = &(SERIAL1_PROTOCOL_TX);

    SerialIO *serial1IO = nullptr;
#endif

SerialIO *serialIO = nullptr;

#define SERIAL_PROTOCOL_RX Serial
#define SERIAL1_PROTOCOL_RX Serial1

StubbornSender TelemetrySender;
static uint8_t telemetryBurstCount;
static uint8_t telemetryBurstMax;

StubbornReceiver MspReceiver;
uint8_t MspData[ELRS_MSP_BUFFER];

uint8_t mavlinkSSBuffer[CRSF_MAX_PACKET_LEN]; // Buffer for current stubbon sender packet (mavlink only)

static bool tlmSent = false;
static uint8_t NextTelemetryType = PACKET_TYPE_LINKSTATS;
static bool telemBurstValid;
/// PFD Filters ////////////////
LPF LPF_Offset(2);
LPF LPF_OffsetDx(4);

/// LQ/RSSI/SNR Calculation //////////
LQCALC<100> LQCalc;
LQCALC<100> LQCalcDVDA;
uint8_t uplinkLQ;
LPF LPF_UplinkRSSI0(5);  // track rssi per antenna
LPF LPF_UplinkRSSI1(5);
MeanAccumulator<int32_t, int8_t, -16> SnrMean;

static uint8_t scanIndex;
uint8_t ExpressLRS_nextAirRateIndex;
int8_t SwitchModePending;

int32_t PfdPrevRawOffset;
RXtimerState_e RXtimerState;
uint32_t GotConnectionMillis = 0;
const uint32_t ConsiderConnGoodMillis = 1000; // minimum time before we can consider a connection to be 'good'
bool doStartTimer = false;

///////////////////////////////////////////////

bool didFHSS = false;
bool alreadyFHSS = false;
bool alreadyTLMresp = false;

//////////////////////////////////////////////////////////////

///////Variables for Telemetry and Link Quality///////////////
uint32_t LastValidPacket = 0;           //Time the last valid packet was recv
uint32_t LastSyncPacket = 0;            //Time the last valid packet was recv

static uint32_t SendLinkStatstoFCintervalLastSent;
static uint8_t SendLinkStatstoFCForcedSends;

int16_t RFnoiseFloor; //measurement of the current RF noise floor
#if defined(DEBUG_RX_SCOREBOARD)
static bool lastPacketCrcError;
#endif
///////////////////////////////////////////////////////////////

/// Variables for Sync Behaviour ////
uint32_t cycleInterval; // in ms
uint32_t RFmodeLastCycled = 0;
#define RFmodeCycleMultiplierSlow 10
uint8_t RFmodeCycleMultiplier;
bool LockRFmode = false;
///////////////////////////////////////

#if defined(DEBUG_BF_LINK_STATS)
// Debug vars
uint8_t debug1 = 0;
uint8_t debug2 = 0;
uint8_t debug3 = 0;
int8_t debug4 = 0;
///////////////////////////////////////
#endif

#if defined(DEBUG_RCVR_LINKSTATS)
static bool debugRcvrLinkstatsPending;
static uint8_t debugRcvrLinkstatsFhssIdx;
#endif

bool BindingModeRequest = false;
#if defined(RADIO_LR1121)
static uint32_t BindingRateChangeTime;
#endif
#define BindingRateChangeCyclePeriod 125

extern void setWifiUpdateMode();
void reconfigureSerial();

uint8_t getLq()
{
    return LQCalc.getLQ();
}

static inline void checkGeminiMode()
{
    if (isDualRadio())
    {
        geminiMode = config.GetAntennaMode() || FHSSuseDualBand; // Force Gemini when in DualBand mode.  Whats the point of using a single frequency!
    }
}

static uint8_t minLqForChaos()
{
    // Determine the most number of CRC-passing packets we could receive on
    // a single channel out of 100 packets that fill the LQcalc span.
    // The LQ must be GREATER THAN this value, not >=
    // The amount of time we coexist on the same channel is
    // 100 divided by the total number of packets in a FHSS loop (rounded up)
    // and there would be 4x packets received each time it passes by so
    // FHSShopInterval * ceil(100 / FHSShopInterval * numfhss) or
    // FHSShopInterval * trunc((100 + (FHSShopInterval * numfhss) - 1) / (FHSShopInterval * numfhss))
    // With a interval of 4 this works out to: 2.4=4, FCC915=4, AU915=8, EU868=8, EU/AU433=36
    const uint32_t numfhss = FHSSgetChannelCount();
    const uint8_t interval = ExpressLRS_currAirRate_Modparams->FHSShopInterval;
    return interval * ((interval * numfhss + 99) / (interval * numfhss));
}

void ICACHE_RAM_ATTR getRFlinkInfo()
{
    int32_t rssiDBM = Radio.LastPacketRSSI;

    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        int32_t rssiDBM2 = Radio.LastPacketRSSI2;

        #if !defined(DEBUG_RCVR_LINKSTATS)
        rssiDBM = LPF_UplinkRSSI0.update(rssiDBM);
        rssiDBM2 = LPF_UplinkRSSI1.update(rssiDBM2);
        #endif
        rssiDBM = (rssiDBM > 0) ? 0 : rssiDBM;
        rssiDBM2 = (rssiDBM2 > 0) ? 0 : rssiDBM2;

        // BetaFlight/iNav expect positive values for -dBm (e.g. -80dBm -> sent as 80)
        CRSF::LinkStatistics.uplink_RSSI_1 = -rssiDBM;
        CRSF::LinkStatistics.uplink_RSSI_2 = -rssiDBM2;
        antenna = (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1) ? 0 : 1;
    }
    else if (antenna == 0)
    {
        #if !defined(DEBUG_RCVR_LINKSTATS)
        rssiDBM = LPF_UplinkRSSI0.update(rssiDBM);
        #endif
        if (rssiDBM > 0) rssiDBM = 0;
        // BetaFlight/iNav expect positive values for -dBm (e.g. -80dBm -> sent as 80)
        CRSF::LinkStatistics.uplink_RSSI_1 = -rssiDBM;
    }
    else
    {
        #if !defined(DEBUG_RCVR_LINKSTATS)
        rssiDBM = LPF_UplinkRSSI1.update(rssiDBM);
        #endif
        if (rssiDBM > 0) rssiDBM = 0;
        // BetaFlight/iNav expect positive values for -dBm (e.g. -80dBm -> sent as 80)
        // May be overwritten below if DEBUG_BF_LINK_STATS is set
        CRSF::LinkStatistics.uplink_RSSI_2 = -rssiDBM;
    }

    SnrMean.add(Radio.LastPacketSNRRaw);

    CRSF::LinkStatistics.active_antenna = antenna;
    CRSF::LinkStatistics.uplink_SNR = SNR_DESCALE(Radio.LastPacketSNRRaw); // possibly overriden below
    //CRSF::LinkStatistics.uplink_Link_quality = uplinkLQ; // handled in Tick
    CRSF::LinkStatistics.rf_Mode = ExpressLRS_currAirRate_Modparams->enum_rate;
    //DBGLN(CRSF::LinkStatistics.uplink_RSSI_1);
    #if defined(DEBUG_BF_LINK_STATS)
    CRSF::LinkStatistics.downlink_RSSI_1 = debug1;
    CRSF::LinkStatistics.downlink_Link_quality = debug2;
    CRSF::LinkStatistics.downlink_SNR = debug3;
    CRSF::LinkStatistics.uplink_RSSI_2 = debug4;
    #endif

    #if defined(DEBUG_RCVR_LINKSTATS)
    // DEBUG_RCVR_LINKSTATS gets full precision SNR, override the value
    CRSF::LinkStatistics.uplink_SNR = Radio.LastPacketSNRRaw;
    debugRcvrLinkstatsFhssIdx = FHSSsequence[FHSSptr];
    #endif
}

void SetRFLinkRate(uint8_t index, bool bindMode) // Set speed of RF link
{
    expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(index);
    expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(index);

    // Binding always uses invertIQ
    bool invertIQ = bindMode || (UID[5] & 0x01);

    uint32_t interval = ModParams->interval;
#if defined(DEBUG_FREQ_CORRECTION) && defined(RADIO_SX128X)
    interval = interval * 12 / 10; // increase the packet interval by 20% to allow adding packet header
#endif

    hwTimer::updateInterval(interval);

    FHSSusePrimaryFreqBand = !(ModParams->radio_type == RADIO_TYPE_LR1121_LORA_2G4) && !(ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_2G4);
    FHSSuseDualBand = ModParams->radio_type == RADIO_TYPE_LR1121_LORA_DUAL;

    Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, FHSSgetInitialFreq(),
                 ModParams->PreambleLen, invertIQ, ModParams->PayloadLength, 0
#if defined(RADIO_SX128X)
                 , uidMacSeedGet(), OtaCrcInitializer, (ModParams->radio_type == RADIO_TYPE_SX128x_FLRC)
#endif
#if defined(RADIO_LR1121)
               , ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_900 || ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_2G4, (uint8_t)UID[5], (uint8_t)UID[4]
#endif
                 );

#if defined(RADIO_LR1121)
    if (FHSSuseDualBand)
    {
        Radio.Config(ModParams->bw2, ModParams->sf2, ModParams->cr2, FHSSgetInitialGeminiFreq(),
                    ModParams->PreambleLen2, invertIQ, ModParams->PayloadLength, 0,
                    ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_900 || ModParams->radio_type == RADIO_TYPE_LR1121_GFSK_2G4,
                    (uint8_t)UID[5], (uint8_t)UID[4], SX12XX_Radio_2);
    }
#endif

    Radio.FuzzySNRThreshold = (RFperf->DynpowerSnrThreshUp == DYNPOWER_SNR_THRESH_NONE) ? 0 : (RFperf->DynpowerSnrThreshDn - RFperf->DynpowerSnrThreshUp);

    checkGeminiMode();
    if (geminiMode)
    {
        Radio.SetFrequencyReg(FHSSgetInitialGeminiFreq(), SX12XX_Radio_2);
    }

    OtaUpdateSerializers(smWideOr8ch, ModParams->PayloadLength);
    MspReceiver.setMaxPackageIndex(ELRS_MSP_MAX_PACKAGES);
    TelemetrySender.setMaxPackageIndex(OtaIsFullRes ? ELRS8_TELEMETRY_MAX_PACKAGES : ELRS4_TELEMETRY_MAX_PACKAGES);

    // Wait for (11/10) 110% of time it takes to cycle through all freqs in FHSS table (in ms)
    cycleInterval = ((uint32_t)11U * FHSSgetChannelCount() * ModParams->FHSShopInterval * interval) / (10U * 1000U);

    ExpressLRS_currAirRate_Modparams = ModParams;
    ExpressLRS_currAirRate_RFperfParams = RFperf;
    ExpressLRS_nextAirRateIndex = index; // presumably we just handled this
    telemBurstValid = false;
}

bool ICACHE_RAM_ATTR HandleFHSS()
{
    uint8_t modresultFHSS = (OtaNonce + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;

    if ((ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0) || alreadyFHSS == true || InBindingMode || (modresultFHSS != 0) || (connectionState == disconnected))
    {
        return false;
    }

    alreadyFHSS = true;

    if (geminiMode)
    {
        if ((((OtaNonce + 1)/ExpressLRS_currAirRate_Modparams->FHSShopInterval) % 2 == 0) || FHSSuseDualBand) // When in DualBand do not switch between radios.  The OTA modulation paramters and HighFreq/LowFreq Tx amps are set during Config.
        {
            Radio.SetFrequencyReg(FHSSgetNextFreq(), SX12XX_Radio_1);
            Radio.SetFrequencyReg(FHSSgetGeminiFreq(), SX12XX_Radio_2);
        }
        else
        {
            // Write radio1 first. This optimises the SPI traffic order.
            uint32_t freqRadio2 = FHSSgetNextFreq();
            Radio.SetFrequencyReg(FHSSgetGeminiFreq(), SX12XX_Radio_1);
            Radio.SetFrequencyReg(freqRadio2, SX12XX_Radio_2);
        }
    }
    else
    {
        Radio.SetFrequencyReg(FHSSgetNextFreq());
    }

#if defined(RADIO_SX127X)
    // SX127x radio has to reset receive mode after hopping
    uint8_t modresultTLM = (OtaNonce + 1) % ExpressLRS_currTlmDenom;
    if (modresultTLM != 0 || ExpressLRS_currTlmDenom == 1) // if we are about to send a tlm response don't bother going back to rx
    {
        Radio.RXnb();
    }
#endif
#if defined(Regulatory_Domain_EU_CE_2400)
    SetClearChannelAssessmentTime();
#endif
    return true;
}

void ICACHE_RAM_ATTR LinkStatsToOta(OTA_LinkStats_s * const ls)
{
    // The value in linkstatistics is "positivized" (inverted polarity)
    // and must be inverted on the TX side. Positive values are used
    // so save a bit to encode which antenna is in use
    ls->uplink_RSSI_1 = CRSF::LinkStatistics.uplink_RSSI_1;
    ls->uplink_RSSI_2 = CRSF::LinkStatistics.uplink_RSSI_2;
    ls->antenna = antenna;
    ls->modelMatch = connectionHasModelMatch;
    ls->lq = CRSF::LinkStatistics.uplink_Link_quality;
    ls->tlmConfirm = MspReceiver.GetCurrentConfirm() ? 1 : 0;
#if defined(DEBUG_FREQ_CORRECTION)
    ls->SNR = FreqCorrection * 127 / FreqCorrectionMax;
#else
    if (SnrMean.getCount())
    {
        ls->SNR = SnrMean.mean();
    }
    else
    {
        ls->SNR = SnrMean.previousMean();
    }
#endif
}

bool ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    uint8_t modresult = (OtaNonce + 1) % ExpressLRS_currTlmDenom;

    if ((connectionState == disconnected) || (ExpressLRS_currTlmDenom == 1) || (alreadyTLMresp == true) || (modresult != 0) || !teamraceHasModelMatch)
    {
        return false; // don't bother sending tlm if disconnected or TLM is off
    }

    // ESP requires word aligned buffer
    WORD_ALIGNED_ATTR OTA_Packet_s otaPkt = {0};
    alreadyTLMresp = true;

    bool tlmQueued = false;
    if (firmwareOptions.is_airport)
    {
        tlmQueued = apInputBuffer.size() > 0;
    }
    else
    {
        tlmQueued = TelemetrySender.IsActive();
    }

    if (NextTelemetryType == PACKET_TYPE_LINKSTATS || !tlmQueued)
    {
        otaPkt.std.type = PACKET_TYPE_LINKSTATS;

        OTA_LinkStats_s * ls;
        if (OtaIsFullRes)
        {
            ls = &otaPkt.full.tlm_dl.ul_link_stats.stats;
            // Include some advanced telemetry in the extra space
            // Note the use of `ul_link_stats.payload` vs just `payload`
            otaPkt.full.tlm_dl.packageIndex = TelemetrySender.GetCurrentPayload(
                otaPkt.full.tlm_dl.ul_link_stats.payload,
                sizeof(otaPkt.full.tlm_dl.ul_link_stats.payload));
        }
        else
        {
            ls = &otaPkt.std.tlm_dl.ul_link_stats.stats;
        }
        LinkStatsToOta(ls);

        NextTelemetryType = PACKET_TYPE_DATA;
        // Start the count at 1 because the next will be DATA and doing +1 before checking
        // against Max below is for some reason 10 bytes more code
        telemetryBurstCount = 1;
    }
    else // if tlmQueued
    {
        if (telemetryBurstCount < telemetryBurstMax)
        {
            telemetryBurstCount++;
        }
        else
        {
            NextTelemetryType = PACKET_TYPE_LINKSTATS;
        }

        otaPkt.std.type = PACKET_TYPE_DATA;

        if (firmwareOptions.is_airport)
        {
            OtaPackAirportData(&otaPkt, &apInputBuffer);
        }
        else
        {
            if (OtaIsFullRes)
            {
                otaPkt.full.tlm_dl.tlmConfirm = MspReceiver.GetCurrentConfirm() ? 1 : 0;
                
                if (TelemetrySender.IsActive())
                {
                    otaPkt.full.tlm_dl.packageIndex = TelemetrySender.GetCurrentPayload(
                        otaPkt.full.tlm_dl.payload,
                        sizeof(otaPkt.full.tlm_dl.payload));
                }
            }
            else
            {
                otaPkt.std.tlm_dl.tlmConfirm = MspReceiver.GetCurrentConfirm() ? 1 : 0;

                if (TelemetrySender.IsActive())
                {
                    otaPkt.std.tlm_dl.packageIndex = TelemetrySender.GetCurrentPayload(
                        otaPkt.std.tlm_dl.payload,
                        sizeof(otaPkt.std.tlm_dl.payload));
                }
            }
        }
    }

    OtaGeneratePacketCrc(&otaPkt);

    SX12XX_Radio_Number_t transmittingRadio;
    if (config.GetForceTlmOff())
    {
        transmittingRadio = SX12XX_Radio_NONE;
    }
    else if (isDualRadio())
    {
        transmittingRadio = SX12XX_Radio_All;
    }
    else
    {
        transmittingRadio = Radio.GetLastSuccessfulPacketRadio();
    }

#if defined(Regulatory_Domain_EU_CE_2400)
    transmittingRadio &= ChannelIsClear(transmittingRadio);   // weed out the radio(s) if channel in use
#endif

    if (!geminiMode && transmittingRadio == SX12XX_Radio_All) // If the receiver is in diversity mode, only send TLM on a single radio.
    {
        transmittingRadio = Radio.LastPacketRSSI > Radio.LastPacketRSSI2 ? SX12XX_Radio_1 : SX12XX_Radio_2; // Pick the radio with best rf connection to the tx.
    }

    Radio.TXnb((uint8_t*)&otaPkt, ExpressLRS_currAirRate_Modparams->PayloadLength, transmittingRadio);

    if (transmittingRadio == SX12XX_Radio_NONE)
    {
        // No packet will be sent due to LBT / Telem forced off.
        // Defer TXdoneCallback() to prepare for TLM when the IRQ is normally triggered.
        deferExecutionMicros(ExpressLRS_currAirRate_RFperfParams->TOA, Radio.TXdoneCallback);
    }

    return true;
}

int32_t ICACHE_RAM_ATTR HandleFreqCorr(bool value)
{
    int32_t tempFC = FreqCorrection;
    if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_2)
    {
        tempFC = FreqCorrection_2;
    }

    if (value)
    {
        if (tempFC > FreqCorrectionMin)
        {
            tempFC--; // FREQ_STEP units
            if (tempFC == FreqCorrectionMin)
            {
                DBGLN("Max -FreqCorrection reached!");
            }
        }
    }
    else
    {
        if (tempFC < FreqCorrectionMax)
        {
            tempFC++; // FREQ_STEP units
            if (tempFC == FreqCorrectionMax)
            {
                DBGLN("Max +FreqCorrection reached!");
            }
        }
    }

    if (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1)
    {
        FreqCorrection = tempFC;
    }
    else
    {
        FreqCorrection_2 = tempFC;
    }

    return tempFC;
}

void ICACHE_RAM_ATTR updatePhaseLock()
{
    if (connectionState != disconnected && PFDloop.hasResult())
    {
        int32_t RawOffset = PFDloop.calcResult();
        int32_t Offset = LPF_Offset.update(RawOffset);
        int32_t OffsetDx = LPF_OffsetDx.update(RawOffset - PfdPrevRawOffset);
        PfdPrevRawOffset = RawOffset;

        if (RXtimerState == tim_locked)
        {
            // limit rate of freq offset adjustment, use slot 1
            // because telemetry can fall on slot 1 and will
            // never get here
            if (OtaNonce % 8 == 1)
            {
                if (Offset > 0)
                {
                    hwTimer::incFreqOffset();
                }
                else if (Offset < 0)
                {
                    hwTimer::decFreqOffset();
                }
            }
        }

        if (connectionState != connected)
        {
            hwTimer::phaseShift(RawOffset >> 1);
        }
        else
        {
            hwTimer::phaseShift(Offset >> 2);
        }

        DBGVLN("%d:%d:%d:%d:%d", Offset, RawOffset, OffsetDx, hwTimer::getFreqOffset(), uplinkLQ);
        UNUSED(OffsetDx); // complier warning if no debug
    }

    PFDloop.reset();
}

void ICACHE_RAM_ATTR HWtimerCallbackTick() // this is 180 out of phase with the other callback, occurs mid-packet reception
{
    updatePhaseLock();
    OtaNonce++;

    // if (!alreadyTLMresp && !alreadyFHSS && !LQCalc.currentIsSet()) // packet timeout AND didn't DIDN'T just hop or send TLM
    // {
    //     Radio.RXnb(); // put the radio cleanly back into RX in case of garbage data
    // }


    if (ExpressLRS_currAirRate_Modparams->numOfSends == 1)
    {
        // Save the LQ value before the inc() reduces it by 1
        uplinkLQ = LQCalc.getLQ();
    } else
    if (!((OtaNonce - 1) % ExpressLRS_currAirRate_Modparams->numOfSends))
    {
        uplinkLQ = LQCalcDVDA.getLQ();
        LQCalcDVDA.inc();
    }

    CRSF::LinkStatistics.uplink_Link_quality = uplinkLQ;
    // Only advance the LQI period counter if we didn't send Telemetry this period
    if (!alreadyTLMresp)
        LQCalc.inc();

    alreadyTLMresp = false;
    alreadyFHSS = false;
}

//////////////////////////////////////////////////////////////
// flip to the other antenna
// no-op if GPIO_PIN_ANT_CTRL not defined
static inline void switchAntenna()
{
    if (GPIO_PIN_ANT_CTRL != UNDEF_PIN && config.GetAntennaMode() == 2)
    {
        // 0 and 1 is use for gpio_antenna_select
        // 2 is diversity
        antenna = !antenna;
        (antenna == 0) ? LPF_UplinkRSSI0.reset() : LPF_UplinkRSSI1.reset(); // discard the outdated value after switching
        digitalWrite(GPIO_PIN_ANT_CTRL, antenna);
        if (GPIO_PIN_ANT_CTRL_COMPL != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_ANT_CTRL_COMPL, !antenna);
        }
    }
}

static void ICACHE_RAM_ATTR updateDiversity()
{

    if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
    {
        if(config.GetAntennaMode() == 2)
        {
            // 0 and 1 is use for gpio_antenna_select
            // 2 is diversity
            static int32_t prevRSSI;        // saved rssi so that we can compare if switching made things better or worse
            static int32_t antennaLQDropTrigger;
            static int32_t antennaRSSIDropTrigger;
            int32_t rssi = (antenna == 0) ? LPF_UplinkRSSI0.value() : LPF_UplinkRSSI1.value();
            int32_t otherRSSI = (antenna == 0) ? LPF_UplinkRSSI1.value() : LPF_UplinkRSSI0.value();

            //if rssi dropped by the amount of DIVERSITY_ANTENNA_RSSI_TRIGGER
            if ((rssi < (prevRSSI - DIVERSITY_ANTENNA_RSSI_TRIGGER)) && antennaRSSIDropTrigger >= DIVERSITY_ANTENNA_INTERVAL)
            {
                switchAntenna();
                antennaLQDropTrigger = 1;
                antennaRSSIDropTrigger = 0;
            }
            else if (rssi > prevRSSI || antennaRSSIDropTrigger < DIVERSITY_ANTENNA_INTERVAL)
            {
                prevRSSI = rssi;
                antennaRSSIDropTrigger++;
            }

            // if we didn't get a packet switch the antenna
            if (!LQCalc.currentIsSet() && antennaLQDropTrigger == 0)
            {
                switchAntenna();
                antennaLQDropTrigger = 1;
                antennaRSSIDropTrigger = 0;
            }
            else if (antennaLQDropTrigger >= DIVERSITY_ANTENNA_INTERVAL)
            {
                // We switched antenna on the previous packet, so we now have relatively fresh rssi info for both antennas.
                // We can compare the rssi values and see if we made things better or worse when we switched
                if (rssi < otherRSSI)
                {
                    // things got worse when we switched, so change back.
                    switchAntenna();
                    antennaLQDropTrigger = 1;
                    antennaRSSIDropTrigger = 0;
                }
                else
                {
                    // all good, we can stay on the current antenna. Clear the flag.
                    antennaLQDropTrigger = 0;
                }
            }
            else if (antennaLQDropTrigger > 0)
            {
                antennaLQDropTrigger ++;
            }
        }
        else
        {
            digitalWrite(GPIO_PIN_ANT_CTRL, config.GetAntennaMode());
            if (GPIO_PIN_ANT_CTRL_COMPL != UNDEF_PIN)
            {
                digitalWrite(GPIO_PIN_ANT_CTRL_COMPL, !config.GetAntennaMode());
            }
            antenna = config.GetAntennaMode();
        }
    }
}

void ICACHE_RAM_ATTR HWtimerCallbackTock()
{
    PFDloop.intEvent(micros()); // our internal osc just fired

    if (ExpressLRS_currAirRate_Modparams->numOfSends > 1 && !(OtaNonce % ExpressLRS_currAirRate_Modparams->numOfSends))
    {
        if (LQCalcDVDA.currentIsSet())
        {
            crsfRCFrameAvailable();
            if (teamraceHasModelMatch)
                servoNewChannelsAvailable();
        }
        else
        {
            crsfRCFrameMissed();
        }
    }
    else if (ExpressLRS_currAirRate_Modparams->numOfSends == 1)
    {
        if (!LQCalc.currentIsSet())
        {
            crsfRCFrameMissed();
        }
    }

    // For any serial drivers that need to send on a regular cadence (i.e. CRSF to betaflight)
    sendImmediateRC();

    if (!didFHSS)
    {
        HandleFHSS();
    }
    didFHSS = false;

    Radio.isFirstRxIrq = true;
    updateDiversity();
    tlmSent = HandleSendTelemetryResponse();

    #if defined(DEBUG_RX_SCOREBOARD)
    static bool lastPacketWasTelemetry = false;
    if (!LQCalc.currentIsSet() && !lastPacketWasTelemetry)
        DBGW(lastPacketCrcError ? '.' : '_');
    lastPacketCrcError = false;
    lastPacketWasTelemetry = tlmSent;
    #endif
}

void LostConnection(bool resumeRx)
{
    DBGLN("lost conn fc=%d fo=%d", FreqCorrection, hwTimer::getFreqOffset());

    // Use this rate as the initial rate next time if we connected on it
    if (connectionState == connected)
        config.SetRateInitialIdx(ExpressLRS_nextAirRateIndex);

    RFmodeCycleMultiplier = 1;
    connectionState = disconnected; //set lost connection
    RXtimerState = tim_disconnected;
    hwTimer::resetFreqOffset();
    PfdPrevRawOffset = 0;
    GotConnectionMillis = 0;
    uplinkLQ = 0;
    LQCalc.reset();
    LQCalcDVDA.reset();
    LPF_Offset.init(0);
    LPF_OffsetDx.init(0);
    alreadyTLMresp = false;
    alreadyFHSS = false;

    if (!InBindingMode)
    {
        if (hwTimer::running)
        {
            while(micros() - PFDloop.getIntEventTime() > 250); // time it just after the tock()
            hwTimer::stop();
        }
        SetRFLinkRate(ExpressLRS_nextAirRateIndex, false); // also sets to initialFreq
        // If not resumRx, Radio will be left in SX127x_OPMODE_STANDBY / SX1280_MODE_STDBY_XOSC
        if (resumeRx)
        {
            Radio.RXnb();
        }
    }
}

void ICACHE_RAM_ATTR TentativeConnection(unsigned long now)
{
    PFDloop.reset();
    connectionState = tentative;
    connectionHasModelMatch = false;
    RXtimerState = tim_disconnected;
    DBGLN("tentative conn");
    PfdPrevRawOffset = 0;
    LPF_Offset.init(0);
    SnrMean.reset();
    RFmodeLastCycled = now; // give another 3 sec for lock to occur

    // The caller MUST call hwTimer::resume(). It is not done here because
    // the timer ISR will fire immediately and preempt any other code
}

void GotConnection(unsigned long now)
{
    if (connectionState == connected)
    {
        return; // Already connected
    }

    LockRFmode = firmwareOptions.lock_on_first_connection;

    connectionState = connected; //we got a packet, therefore no lost connection
    RXtimerState = tim_tentative;
    GotConnectionMillis = now;
    #if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    webserverPreventAutoStart = true;
    #endif

    if (firmwareOptions.is_airport)
    {
        apInputBuffer.flush();
        apOutputBuffer.flush();
    }

    DBGLN("got conn");
}

static void ICACHE_RAM_ATTR ProcessRfPacket_RC(OTA_Packet_s const * const otaPktPtr)
{
    // Must be fully connected to process RC packets, prevents processing RC
    // during sync, where packets can be received before connection
    if (connectionState != connected || SwitchModePending)
        return;

    bool telemetryConfirmValue = OtaUnpackChannelData(otaPktPtr, ChannelData, ExpressLRS_currTlmDenom);
    TelemetrySender.ConfirmCurrentPayload(telemetryConfirmValue);

    // No channels packets to the FC or PWM pins if no model match
    if (connectionHasModelMatch)
    {
        if (ExpressLRS_currAirRate_Modparams->numOfSends == 1)
        {
            crsfRCFrameAvailable();
            // teamrace is only checked for servos because the teamrace model select logic only runs
            // when new frames are available, and will decide later if the frame will be forwarded
            if (teamraceHasModelMatch)
                servoNewChannelsAvailable();
        }
        else if (!LQCalcDVDA.currentIsSet())
        {
            LQCalcDVDA.add();
        }
        #if defined(DEBUG_RCVR_LINKSTATS)
        debugRcvrLinkstatsPending = true;
        #endif
    }
}

void ICACHE_RAM_ATTR OnELRSBindMSP(uint8_t* newUid4)
{
    // Binding over MSP only contains 4 bytes due to packet size limitations, clear out any leading bytes
    UID[0] = 0;
    UID[1] = 0;
    for (unsigned i = 0; i < 4; i++)
    {
        UID[i + 2] = newUid4[i];
    }

    DBGLN("New UID = %u, %u, %u, %u, %u, %u", UID[0], UID[1], UID[2], UID[3], UID[4], UID[5]);

    // Set new UID in eeprom
    // EEPROM commit will happen on the main thread in ExitBindingMode()
    config.SetUID(UID);
}

static void ICACHE_RAM_ATTR ProcessRfPacket_MSP(OTA_Packet_s const * const otaPktPtr)
{
    uint8_t packageIndex;
    uint8_t const * payload;
    uint8_t dataLen;
    if (OtaIsFullRes)
    {
        packageIndex = otaPktPtr->full.msp_ul.packageIndex;
        payload = otaPktPtr->full.msp_ul.payload;
        dataLen = sizeof(otaPktPtr->full.msp_ul.payload);
        if (config.GetSerialProtocol() == PROTOCOL_MAVLINK)
        {
            TelemetrySender.ConfirmCurrentPayload(otaPktPtr->full.msp_ul.tlmConfirm);
        }
        else
        {
            packageIndex &= ELRS8_TELEMETRY_MAX_PACKAGES;
        }
    }
    else
    {
        packageIndex = otaPktPtr->std.msp_ul.packageIndex;
        payload = otaPktPtr->std.msp_ul.payload;
        dataLen = sizeof(otaPktPtr->std.msp_ul.payload);
        if (config.GetSerialProtocol() == PROTOCOL_MAVLINK)
        {
            TelemetrySender.ConfirmCurrentPayload(otaPktPtr->std.msp_ul.tlmConfirm);
        }
        else
        {
            packageIndex &= ELRS4_TELEMETRY_MAX_PACKAGES;
        }
    }

    // Always examine MSP packets for bind information if in bind mode
    // [1] is the package index, first packet of the MSP
    if (InBindingMode && packageIndex == 1 && payload[0] == MSP_ELRS_BIND)
    {
        OnELRSBindMSP((uint8_t *)&payload[1]);
        return;
    }

    // Must be fully connected to process MSP, prevents processing MSP
    // during sync, where packets can be received before connection
    if (connectionState == connected)
    {
        MspReceiver.ReceiveData(packageIndex, payload, dataLen);
    }
}

static void ICACHE_RAM_ATTR updateSwitchModePendingFromOta(uint8_t newSwitchMode)
{
    if (OtaSwitchModeCurrent == newSwitchMode)
    {
        // Cancel any switch if pending
        SwitchModePending = 0;
        return;
    }

    // One is added to the mode because SwitchModePending==0 means no switch pending
    // and that's also a valid switch mode. The 1 is removed when this is handled.
    // A negative SwitchModePending means not to switch yet
    int8_t newSwitchModePending = -(int8_t)newSwitchMode - 1;

    // Switch mode can be changed while disconnected
    // OR there are two sync packets with the same new switch mode,
    // as a "confirm". No RC packets are processed until
    if (connectionState == disconnected ||
        SwitchModePending == newSwitchModePending)
    {
        // Add one to the mode because SwitchModePending==0 means no switch pending
        // and that's also a valid switch mode. The 1 is removed when this is handled
        SwitchModePending = newSwitchMode + 1;
    }
    else
    {
        // Save the negative version of the new switch mode to compare
        // against on the next SYNC packet, but do not switch yet
        SwitchModePending = newSwitchModePending;
    }
}

static bool ICACHE_RAM_ATTR ProcessRfPacket_SYNC(uint32_t const now, OTA_Sync_s const * const otaSync)
{
    // Verify the first byte of the binding ID, which should always match
    if (otaSync->UID4 != UID[4])
        return false;

    // The third byte will be XORed with inverse of the ModelId if ModelMatch is on
    // Only require the first 18 bits of the UID to match to establish a connection
    // but the last 6 bits must modelmatch before sending any data to the FC
    if ((otaSync->UID5 & ~MODELMATCH_MASK) != (UID[5] & ~MODELMATCH_MASK))
        return false;

    LastSyncPacket = now;
#if defined(DEBUG_RX_SCOREBOARD)
    DBGW('s');
#endif

    if (isDualRadio())
    {
        config.SetAntennaMode(otaSync->geminiMode);
    }

    // Will change the packet air rate in loop() if this changes
    ExpressLRS_nextAirRateIndex = enumRatetoIndex((expresslrs_RFrates_e)otaSync->rfRateEnum);
    updateSwitchModePendingFromOta(otaSync->switchEncMode);

    // Update TLM ratio, should never be TLM_RATIO_STD/DISARMED, the TX calculates the correct value for the RX
    expresslrs_tlm_ratio_e TLMrateIn = (expresslrs_tlm_ratio_e)(otaSync->newTlmRatio + (uint8_t)TLM_RATIO_NO_TLM);
    uint8_t TlmDenom = TLMratioEnumToValue(TLMrateIn);
    if (ExpressLRS_currTlmDenom != TlmDenom)
    {
        DBGLN("New TLMrate 1:%u", TlmDenom);
        ExpressLRS_currTlmDenom = TlmDenom;
        telemBurstValid = false;
    }

    // modelId = 0xff indicates modelMatch is disabled, the XOR does nothing in that case
    uint8_t modelXor = (~config.GetModelId()) & MODELMATCH_MASK;
    bool modelMatched = otaSync->UID5 == (UID[5] ^ modelXor);
    DBGVLN("MM %u=%u %d", otaSync->UID5, UID[5], modelMatched);

    if (connectionState == disconnected
        || OtaNonce != otaSync->nonce
        || FHSSgetCurrIndex() != otaSync->fhssIndex
        || connectionHasModelMatch != modelMatched)
    {
        //DBGLN("\r\n%ux%ux%u", OtaNonce, otaPktPtr->sync.nonce, otaPktPtr->sync.fhssIndex);
        FHSSsetCurrIndex(otaSync->fhssIndex);
        OtaNonce = otaSync->nonce;
        TentativeConnection(now);
        // connectionHasModelMatch must come after TentativeConnection, which resets it
        connectionHasModelMatch = modelMatched;
        return true;
    }

    return false;
}

bool ICACHE_RAM_ATTR ProcessRFPacket(SX12xxDriverCommon::rx_status const status)
{
    if (status != SX12xxDriverCommon::SX12XX_RX_OK)
    {
        DBGVLN("HW CRC error");
        #if defined(DEBUG_RX_SCOREBOARD)
            lastPacketCrcError = true;
        #endif
        return false;
    }
    uint32_t const beginProcessing = micros();

    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s * const)Radio.RXdataBuffer;
    if (!OtaValidatePacketCrc(otaPktPtr))
    {
        DBGVLN("CRC error");
        #if defined(DEBUG_RX_SCOREBOARD)
            lastPacketCrcError = true;
        #endif
        return false;
    }

    PFDloop.extEvent(beginProcessing + PACKET_TO_TOCK_SLACK);

    doStartTimer = false;
    unsigned long now = millis();

    LastValidPacket = now;

    switch (otaPktPtr->std.type)
    {
    case PACKET_TYPE_RCDATA: //Standard RC Data Packet
        ProcessRfPacket_RC(otaPktPtr);
        break;
    case PACKET_TYPE_SYNC: //sync packet from master
        doStartTimer = ProcessRfPacket_SYNC(now,
            OtaIsFullRes ? &otaPktPtr->full.sync.sync : &otaPktPtr->std.sync)
            && !InBindingMode;
        break;
    case PACKET_TYPE_DATA:
        if (firmwareOptions.is_airport)
        {
            OtaUnpackAirportData(otaPktPtr, &apOutputBuffer);
        }
        else
        {
            ProcessRfPacket_MSP(otaPktPtr);
        }
        break;
    default:
        break;
    }

    // Store the LQ/RSSI/Antenna
    Radio.GetLastPacketStats();
    getRFlinkInfo();

    if (Radio.FrequencyErrorAvailable())
    {
    #if defined(RADIO_SX127X)
        // Adjusts FreqCorrection for RX freq offset
        int32_t tempFreqCorrection = HandleFreqCorr(Radio.GetFrequencyErrorbool());
        // Teamp900 also needs to adjust its demood PPM
        Radio.SetPPMoffsetReg(tempFreqCorrection);
    #else /* !RADIO_SX127X */
        // Adjusts FreqCorrection for RX freq offset
        HandleFreqCorr(Radio.GetFrequencyErrorbool());
    #endif /* RADIO_SX127X */
    }

    // Received a packet, that's the definition of LQ
    LQCalc.add();
    // Extend sync duration since we've received a packet at this rate
    // but do not extend it indefinitely
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow;

#if defined(DEBUG_RX_SCOREBOARD)
    if (otaPktPtr->std.type != PACKET_TYPE_SYNC) DBGW(connectionHasModelMatch ? 'R' : 'r');
#endif

    return true;
}

bool ICACHE_RAM_ATTR RXdoneISR(SX12xxDriverCommon::rx_status const status)
{
    if (LQCalc.currentIsSet() && connectionState == connected)
    {
        return false; // Already received a packet, do not run ProcessRFPacket() again.
    }

    if (ProcessRFPacket(status))
    {
        didFHSS = HandleFHSS();

        if (doStartTimer)
        {
            doStartTimer = false;
            hwTimer::resume(); // will throw an interrupt immediately
        }

        return true;
    }
    return false;
}

void ICACHE_RAM_ATTR TXdoneISR()
{
    Radio.RXnb();
#if defined(Regulatory_Domain_EU_CE_2400)
    SetClearChannelAssessmentTime();
#endif
#if defined(DEBUG_RX_SCOREBOARD)
    DBGW('T');
#endif
}

void UpdateModelMatch(uint8_t model)
{
    DBGLN("Set ModelId=%u", model);
    config.SetModelId(model);
}

void SendMSPFrameToFC(uint8_t *mspData)
{
    serialIO->queueMSPFrameTransmission(mspData);
}

/**
 * Process the assembled MSP packet in MspData[]
 **/
void MspReceiveComplete()
{
    switch (MspData[0])
    {
    case MSP_ELRS_SET_RX_WIFI_MODE: //0x0E
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
        // The MSP packet needs to be ACKed so the TX doesn't
        // keep sending it, so defer the switch to wifi
        deferExecutionMillis(500, []() {
            setWifiUpdateMode();
        });
#endif
        break;
    case MSP_ELRS_MAVLINK_TLM: // 0xFD
        // raw mavlink data
        mavlinkOutputBuffer.atomicPushBytes(&MspData[2], MspData[1]);
        break;
    default:
        //handle received CRSF package
        crsf_ext_header_t *receivedHeader = (crsf_ext_header_t *) MspData;
        switch (receivedHeader->type)
        {
        case CRSF_FRAMETYPE_MSP_WRITE: //encapsulated MSP payload
            if (MspData[7] == MSP_SET_RX_CONFIG && MspData[8] == MSP_ELRS_MODEL_ID)
            {
                UpdateModelMatch(MspData[9]);
                break;
            }
            else if (MspData[7] == MSP_SET_VTX_CONFIG)
            {
                if (OPT_HAS_VTX_SPI) {
                    vtxSPIFrequency = getFreqByIdx(MspData[8]);
                    if (MspData[6] >= 4) // If packet has 4 bytes it also contains power idx and pitmode.
                    {
                        vtxSPIPowerIdx = MspData[10];
                        vtxSPIPitmode = MspData[11];
                    }
                    devicesTriggerEvent();
                    break;
#if defined(PLATFORM_ESP32)
                } else if (config.GetSerial1Protocol() == PROTOCOL_SERIAL1_TRAMP || config.GetSerial1Protocol() == PROTOCOL_SERIAL1_SMARTAUDIO) {
                    serial1IO->queueMSPFrameTransmission(MspData);
                    break;
#endif
                }
            }
            // FALLTHROUGH
        default:
            if ((receivedHeader->dest_addr == CRSF_ADDRESS_BROADCAST || receivedHeader->dest_addr == CRSF_ADDRESS_CRSF_RECEIVER))
            {
                luaParamUpdateReq(
                    MspData[CRSF_TELEMETRY_TYPE_INDEX],
                    MspData[CRSF_TELEMETRY_FIELD_ID_INDEX],
                    MspData[CRSF_TELEMETRY_FIELD_CHUNK_INDEX]
                );
            }
            break;
        }
        // No MSP data to the FC if no model match
        if (connectionHasModelMatch && teamraceHasModelMatch &&
            (receivedHeader->dest_addr == CRSF_ADDRESS_BROADCAST || receivedHeader->dest_addr == CRSF_ADDRESS_FLIGHT_CONTROLLER))
        {
            serialIO->queueMSPFrameTransmission(MspData);
        }
    }

    MspReceiver.Unlock();
}

static void setupSerial()
{
    bool sbusSerialOutput = false;
	bool sumdSerialOutput = false;
    bool mavlinkSerialOutput = false;

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
    bool hottTlmSerial = false;
#endif

    if (OPT_CRSF_RCVR_NO_SERIAL)
    {
        // For PWM receivers with no serial pins defined, only turn on the Serial port if logging is on
        #if defined(DEBUG_LOG) || defined(DEBUG_RCVR_LINKSTATS)
        #if defined(PLATFORM_ESP32_S3) && !defined(ESP32_S3_USB_JTAG_ENABLED)
        // Requires pull-down on GPIO3.  If GPIO3 has a pull-up (for JTAG) this doesn't work.
        USBSerial.begin(serialBaud);
        SerialLogger = &USBSerial;
        #else
        Serial.begin(serialBaud);
        SerialLogger = &Serial;
        #endif
        #else
        SerialLogger = new NullStream();
        #endif
        serialIO = new SerialNOOP();
        return;
    }
    if (config.GetSerialProtocol() == PROTOCOL_CRSF || config.GetSerialProtocol() == PROTOCOL_INVERTED_CRSF || firmwareOptions.is_airport)
    {
        serialBaud = firmwareOptions.uart_baud;
    }
    else if (config.GetSerialProtocol() == PROTOCOL_SBUS || config.GetSerialProtocol() == PROTOCOL_INVERTED_SBUS || config.GetSerialProtocol() == PROTOCOL_DJI_RS_PRO)
    {
        sbusSerialOutput = true;
        serialBaud = 100000;
    }
    else if (config.GetSerialProtocol() == PROTOCOL_SUMD)
    {
        sumdSerialOutput = true;
        serialBaud = 115200;
    }
    else if (config.GetSerialProtocol() == PROTOCOL_MAVLINK)
    {
        mavlinkSerialOutput = true;
        serialBaud = 460800;
    }
    else if (config.GetSerialProtocol() == PROTOCOL_MSP_DISPLAYPORT)
    {
        serialBaud = 115200;
    }
#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
    else if (config.GetSerialProtocol() == PROTOCOL_HOTT_TLM)
    {
        hottTlmSerial = true;
        serialBaud = 19200;
    }
#endif
    bool invert = config.GetSerialProtocol() == PROTOCOL_SBUS || config.GetSerialProtocol() == PROTOCOL_INVERTED_CRSF || config.GetSerialProtocol() == PROTOCOL_DJI_RS_PRO;

#if defined(PLATFORM_ESP8266)
    SerialConfig serialConfig = SERIAL_8N1;

    if(sbusSerialOutput)
    {
        serialConfig = SERIAL_8E2;
    }
    else if(hottTlmSerial)
    {
        serialConfig = SERIAL_8N2;
    }

    SerialMode mode = (sbusSerialOutput || sumdSerialOutput)  ? SERIAL_TX_ONLY : SERIAL_FULL;
    Serial.begin(serialBaud, serialConfig, mode, -1, invert);
#elif defined(PLATFORM_ESP32)
    uint32_t serialConfig = SERIAL_8N1;

    if(sbusSerialOutput)
    {
        serialConfig = SERIAL_8E2;
    }
    else if(hottTlmSerial)
    {
        serialConfig = SERIAL_8N2;
    }

    // ARDUINO_CORE_INVERT_FIX PT2
    #if defined(ARDUINO_CORE_INVERT_FIX)
    if(invert == false)
    {
        uart_set_line_inverse(0, UART_SIGNAL_INV_DISABLE);
    }
    #endif
    // ARDUINO_CORE_INVERT_FIX PT2 end

    Serial.begin(serialBaud, serialConfig, GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX, invert);
#endif

    if (firmwareOptions.is_airport)
    {
        serialIO = new SerialAirPort(SERIAL_PROTOCOL_TX, SERIAL_PROTOCOL_RX);
    }
    else if (sbusSerialOutput)
    {
        serialIO = new SerialSBUS(SERIAL_PROTOCOL_TX, SERIAL_PROTOCOL_RX);
    }
    else if (sumdSerialOutput)
    {
        serialIO = new SerialSUMD(SERIAL_PROTOCOL_TX, SERIAL_PROTOCOL_RX);
    }
    else if (mavlinkSerialOutput)
    {
        serialIO = new SerialMavlink(SERIAL_PROTOCOL_TX, SERIAL_PROTOCOL_RX);
    }
    else if (config.GetSerialProtocol() == PROTOCOL_MSP_DISPLAYPORT)
    {
        serialIO = new SerialDisplayport(SERIAL_PROTOCOL_TX, SERIAL_PROTOCOL_RX);
    }
    else if (hottTlmSerial)
    {
        serialIO = new SerialHoTT_TLM(SERIAL_PROTOCOL_TX, SERIAL_PROTOCOL_RX);
    }
    else
    {
        serialIO = new SerialCRSF(SERIAL_PROTOCOL_TX, SERIAL_PROTOCOL_RX);
    }

#if defined(DEBUG_ENABLED)
#if defined(PLATFORM_ESP32_S3) || defined(PLATFORM_ESP32_C3)
    USBSerial.begin(460800);
    SerialLogger = &USBSerial;
#else
    SerialLogger = &Serial;
#endif
#else
    SerialLogger = new NullStream();
#endif
}

#if defined(PLATFORM_ESP32)
static void serial1Shutdown()
{
    if(serial1IO != nullptr)
    {
        Serial1.end();
        delete serial1IO;
        serial1IO = nullptr;
    }
}

static void setupSerial1()
{
    //
    // init secondary serial and protocol
    //
    int8_t serial1RXpin = GPIO_PIN_SERIAL1_RX;

    if (serial1RXpin == UNDEF_PIN)
    {
        for (uint8_t ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ch++)
        {
            if (config.GetPwmChannel(ch)->val.mode == somSerial1RX)
                serial1RXpin = GPIO_PIN_PWM_OUTPUTS[ch];
        }
    }

    int8_t serial1TXpin = GPIO_PIN_SERIAL1_TX;

    if (serial1TXpin == UNDEF_PIN)
    {
        for (uint8_t ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ch++)
        {
            if (config.GetPwmChannel(ch)->val.mode == somSerial1TX)
                serial1TXpin = GPIO_PIN_PWM_OUTPUTS[ch];
        }
    }

    switch(config.GetSerial1Protocol())
    {
        case PROTOCOL_SERIAL1_OFF:
            break;
        case PROTOCOL_SERIAL1_CRSF:
            Serial1.begin(firmwareOptions.uart_baud, SERIAL_8N1, serial1RXpin, serial1TXpin, false);
            serial1IO = new SerialCRSF(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX);
            break;
        case PROTOCOL_SERIAL1_INVERTED_CRSF:
            Serial1.begin(firmwareOptions.uart_baud, SERIAL_8N1, serial1RXpin, serial1TXpin, true);
            serial1IO = new SerialCRSF(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX);
            break;
        case PROTOCOL_SERIAL1_SBUS:
        case PROTOCOL_SERIAL1_DJI_RS_PRO:
            Serial1.begin(100000, SERIAL_8E2, UNDEF_PIN, serial1TXpin, true);
            serial1IO = new SerialSBUS(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX);
            break;
        case PROTOCOL_SERIAL1_INVERTED_SBUS:
            Serial1.begin(100000, SERIAL_8E2, UNDEF_PIN, serial1TXpin, false);
            serial1IO = new SerialSBUS(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX);
            break;
        case PROTOCOL_SERIAL1_SUMD:
            Serial1.begin(115200, SERIAL_8N1, UNDEF_PIN, serial1TXpin, false);
            serial1IO = new SerialSUMD(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX);
            break;
        case PROTOCOL_SERIAL1_HOTT_TLM:
            Serial1.begin(19200, SERIAL_8N2, serial1RXpin, serial1TXpin, false);
            serial1IO = new SerialHoTT_TLM(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX, serial1TXpin);
            break;
        case PROTOCOL_SERIAL1_TRAMP:
            Serial1.begin(9600, SERIAL_8N1, UNDEF_PIN, serial1TXpin, false);
            serial1IO = new SerialTramp(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX, serial1TXpin);
            break;
        case PROTOCOL_SERIAL1_SMARTAUDIO:
            Serial1.begin(4800, SERIAL_8N2, UNDEF_PIN, serial1TXpin, false);
            serial1IO = new SerialSmartAudio(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX, serial1TXpin);
            break;
        case PROTOCOL_SERIAL1_MSP_DISPLAYPORT:
            Serial1.begin(115200, SERIAL_8N1, UNDEF_PIN, serial1TXpin, false);
            serial1IO = new SerialDisplayport(SERIAL1_PROTOCOL_TX, SERIAL1_PROTOCOL_RX);
            break;
    }
}

void reconfigureSerial1()
{
    serial1Shutdown();
    setupSerial1();
}
#else
    void setupSerial1() {};
    void reconfigureSerial1() {};
#endif

static void serialShutdown()
{
    SerialLogger = new NullStream();
    if(serialIO != nullptr)
    {
        Serial.end();
        delete serialIO;
        serialIO = nullptr;
    }
}

void reconfigureSerial()
{
    serialShutdown();
    setupSerial();
}

static void setupConfigAndPocCheck()
{
    eeprom.Begin();
    config.SetStorageProvider(&eeprom); // Pass pointer to the Config class for access to storage
    config.Load();

    // If bound, track number of plug/unplug cycles to go to binding mode in eeprom
    if (config.GetIsBound() && config.GetPowerOnCounter() < 3)
    {
        config.SetPowerOnCounter(config.GetPowerOnCounter() + 1);
        config.Commit();
    }

    // Set a deferred function to clear the power on counter if the RX has been running for more than 2s
    deferExecutionMillis(2000, []() {
        if (connectionState != connected && config.GetPowerOnCounter() != 0)
        {
            config.SetPowerOnCounter(0);
            config.Commit();
        }
    });
}

static void setupTarget()
{
    if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_ANT_CTRL, OUTPUT);
        digitalWrite(GPIO_PIN_ANT_CTRL, LOW);
        if (GPIO_PIN_ANT_CTRL_COMPL != UNDEF_PIN)
        {
            pinMode(GPIO_PIN_ANT_CTRL_COMPL, OUTPUT);
            digitalWrite(GPIO_PIN_ANT_CTRL_COMPL, HIGH);
        }
    }

    setupTargetCommon();
}

static void setupBindingFromConfig()
{
    // VolatileBind's only function is to prevent loading the stored UID into RAM
    // which makes the RX boot into bind mode every time
    if (config.GetIsBound())
    {
        memcpy(UID, config.GetUID(), UID_LEN);
    }

    DBGLN("UID=(%d, %d, %d, %d, %d, %d) ModelId=%u",
        UID[0], UID[1], UID[2], UID[3], UID[4], UID[5], config.GetModelId());

    OtaUpdateCrcInitFromUid();
}

static void setupRadio()
{
    Radio.currFreq = FHSSgetInitialFreq();
#if defined(RADIO_SX127X)
    //Radio.currSyncWord = UID[3];
#endif
    bool init_success = Radio.Begin(FHSSgetMinimumFreq(), FHSSgetMaximumFreq());
    POWERMGNT::init();
    if (!init_success)
    {
        DBGLN("Failed to detect RF chipset!!!");
        connectionState = radioFailed;
        return;
    }

    DynamicPower_UpdateRx(true);

#if defined(Regulatory_Domain_EU_CE_2400)
    LBTEnabled = (config.GetPower() > PWR_10mW);
#endif

    Radio.RXdoneCallback = &RXdoneISR;
    Radio.TXdoneCallback = &TXdoneISR;

    scanIndex = config.GetRateInitialIdx();
    for (int i=0 ; i<RATE_MAX ; i++)
    {
        if (isSupportedRFRate(scanIndex))
        {
            break;
        }
        scanIndex = (scanIndex + 1) % RATE_MAX;
    }
    SetRFLinkRate(scanIndex, false);
    // Start slow on the selected rate to give it the best chance
    // to connect before beginning rate cycling
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow / 2;
}

static void updateTelemetryBurst()
{
    if (telemBurstValid)
        return;
    telemBurstValid = true;

    uint16_t hz = 1000000 / ExpressLRS_currAirRate_Modparams->interval;
    telemetryBurstMax = TLMBurstMaxForRateRatio(hz, ExpressLRS_currTlmDenom);

    // Notify the sender to adjust its expected throughput
    TelemetrySender.UpdateTelemetryRate(hz, ExpressLRS_currTlmDenom, telemetryBurstMax);
}

/* If not connected will rotate through the RF modes looking for sync
 * and blink LED
 */
static void cycleRfMode(unsigned long now)
{
    if (connectionState == connected || connectionState == wifiUpdate || InBindingMode)
        return;

    // Actually cycle the RF mode if not LOCK_ON_FIRST_CONNECTION
    if (LockRFmode == false && (now - RFmodeLastCycled) > (cycleInterval * RFmodeCycleMultiplier))
    {
        RFmodeLastCycled = now;
        LastSyncPacket = now;           // reset this variable
        SendLinkStatstoFCForcedSends = 2;
        SetRFLinkRate(scanIndex % RATE_MAX, false); // switch between rates
        LQCalc.reset100();
        LQCalcDVDA.reset100();
        // Display the current air rate to the user as an indicator something is happening
        scanIndex++;
        Radio.RXnb();
        DBGLN("%u", ExpressLRS_currAirRate_Modparams->interval);

        // Skip unsupported modes for hardware with only a single LR1121 or with a single RF path
        while (!isSupportedRFRate(scanIndex % RATE_MAX))
        {
            DBGLN("Skip %u", get_elrs_airRateConfig(scanIndex % RATE_MAX)->interval);
            scanIndex++;
        }

        // Switch to FAST_SYNC if not already in it (won't be if was just connected)
        RFmodeCycleMultiplier = 1;
    } // if time to switch RF mode
}

static void EnterBindingMode()
{
    if (InBindingMode)
    {
        DBGLN("Already in binding mode");
        return;
    }

    // Binding uses 50Hz, and InvertIQ
    OtaCrcInitializer = OTA_VERSION_ID;
    InBindingMode = true;
    // Any method of entering bind resets a loan
    // Model can be reloaned immediately by binding now
    config.ReturnLoan();
    config.Commit();

    // Start attempting to bind
    // Lock the RF rate and freq while binding
    SetRFLinkRate(enumRatetoIndex(RATE_BINDING), true);

    // If the Radio Params (including InvertIQ) parameter changed, need to restart RX to take effect
    Radio.RXnb();

    DBGLN("Entered binding mode at freq = %d", Radio.currFreq);
    devicesTriggerEvent();
}

static void ExitBindingMode()
{
    if (!InBindingMode)
    {
        DBGLN("Not in binding mode");
        return;
    }

    MspReceiver.ResetState();

    // Prevent any new packets from coming in
    Radio.SetTxIdleMode();
    // Write the values to eeprom
    config.Commit();

    OtaUpdateCrcInitFromUid();
    FHSSrandomiseFHSSsequence(uidMacSeedGet());

    #if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    webserverPreventAutoStart = true;
    #endif

    // Force RF cycling to start at the beginning immediately
    scanIndex = RATE_MAX;
    RFmodeLastCycled = 0;
    LockRFmode = false;
    LostConnection(false);

    // Do this last as LostConnection() will wait for a tock that never comes
    // if we're in binding mode
    InBindingMode = false;
    DBGLN("Exiting binding mode");
    devicesTriggerEvent();
}

static void updateBindingMode(unsigned long now)
{
    // Exit binding mode if the config has been modified, indicating UID has been set
    if (InBindingMode && config.IsModified())
    {
        ExitBindingMode();
    }

#if defined(RADIO_LR1121)
    // Change frequency domains every 500ms.  This will allow single LR1121 receivers to receive bind packets from SX12XX Tx modules.
    else if (InBindingMode && (now - BindingRateChangeTime) > BindingRateChangeCyclePeriod)
    {
        BindingRateChangeTime = now;
        if (ExpressLRS_currAirRate_Modparams->index == RATE_DUALBAND_BINDING)
        {
            SetRFLinkRate(enumRatetoIndex(RATE_BINDING), true);
        }
        else
        {
            SetRFLinkRate(RATE_DUALBAND_BINDING, true);
        }

        Radio.RXnb();
    }
#endif

    // If the power on counter is >=3, enter binding, the counter will be reset after 2s
    else if (!InBindingMode && config.GetPowerOnCounter() >= 3)
    {
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
        // Never enter wifi if forced to binding mode
        webserverPreventAutoStart = true;
#endif
        DBGLN("Power on counter >=3, enter binding mode");
        EnterBindingMode();
    }

    // If the eeprom is indicating that we're not bound, enter binding
    else if (!UID_IS_BOUND(UID) && !InBindingMode)
    {
        DBGLN("RX has not been bound, enter binding mode");
        EnterBindingMode();
    }

    else if (BindingModeRequest)
    {
        DBGLN("Connected request to enter binding mode");
        BindingModeRequest = false;
        if (connectionState == connected)
        {
            LostConnection(false);
            // Skip entering bind mode if on loan. This comes from an OTA request
            // and the model is assumed to be inaccessible, do not want the receiver
            // sitting in a field ready to be bound to anyone within 10km
            if (config.IsOnLoan())
            {
                DBGLN("Model was on loan, becoming inert");
                config.ReturnLoan();
                config.Commit(); // prevents CheckConfigChangePending() re-enabling radio
                Radio.End();
                // Enter a completely invalid state for a receiver, to prevent wifi or radio enabling
                connectionState = noCrossfire;
                return;
            }
            // if the InitRate config item was changed by LostConnection
            // save the config before entering bind, as the modified config
            // will immediately boot it out of bind mode
            config.Commit();
        }
        EnterBindingMode();
    }
}

void EnterBindingModeSafely()
{
    // Will not enter Binding mode if in the process of a passthrough update
    // or currently binding
    if (connectionState == serialUpdate || InBindingMode)
        return;

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    // Never enter wifi mode after requesting to enter binding mode
    webserverPreventAutoStart = true;

    // If the radio and everything is shut down, better to reboot and boot to binding mode
    if (connectionState == wifiUpdate || connectionState == bleJoystick)
    {
        // Force 3-plug binding mode
        config.SetPowerOnCounter(3);
        config.Commit();
        ESP.restart();
        // Unreachable
    }
#endif

    // If connected, handle that in updateBindingMode()
    if (connectionState == connected)
    {
        BindingModeRequest = true;
        return;
    }

    EnterBindingMode();
}

static void checkSendLinkStatsToFc(uint32_t now)
{
    if (now - SendLinkStatstoFCintervalLastSent > SEND_LINK_STATS_TO_FC_INTERVAL)
    {
        if (connectionState == disconnected)
        {
            getRFlinkInfo();
        }

        if ((connectionState != disconnected && connectionHasModelMatch && teamraceHasModelMatch) ||
            SendLinkStatstoFCForcedSends)
        {
            serialIO->queueLinkStatisticsPacket();
            SendLinkStatstoFCintervalLastSent = now;
            if (SendLinkStatstoFCForcedSends)
                --SendLinkStatstoFCForcedSends;
        }
    }
}

static void debugRcvrLinkstats()
{
#if defined(DEBUG_RCVR_LINKSTATS)
    if (debugRcvrLinkstatsPending)
    {
        debugRcvrLinkstatsPending = false;

        // Copy the data out of the ISR-updating bits ASAP
        // While YOLOing (const void *) away the volatile
        crsfLinkStatistics_t ls = *(crsfLinkStatistics_t *)((const void *)&CRSF::LinkStatistics);
        uint32_t packetCounter = debugRcvrLinkstatsPacketId;
        uint8_t fhss = debugRcvrLinkstatsFhssIdx;
        // actually the previous packet's offset since the update happens in tick, and this will
        // fire right after packet reception (a little before tock)
        int32_t pfd = PfdPrevRawOffset;

        // Use serial instead of DBG() because do not necessarily want all the debug in our logs
        char buf[50];
        snprintf(buf, sizeof(buf), "%u,%u,-%u,%u,%d,%u,%u,%d\r\n",
            packetCounter, ls.active_antenna,
            ls.active_antenna ? ls.uplink_RSSI_2 : ls.uplink_RSSI_1,
            ls.uplink_Link_quality, ls.uplink_SNR,
            ls.uplink_TX_Power, fhss, pfd);
        Serial.write(buf);
    }
#endif
}

static void debugRcvrSignalStats(uint32_t now)
{
#if defined(DEBUG_RCVR_SIGNAL_STATS)
    static uint32_t lastReport = 0;

    // log column header:  cnt1, rssi1, snr1, snr1_max, telem1, fail1, cnt2, rssi2, snr2, snr2_max, telem2, fail2, or, both
    if(now - lastReport >= 1000 && connectionState == connected)
    {
        for (int i = 0 ; i < (isDualRadio()?2:1) ; i++)
        {
            DBG("%d\t%f\t%f\t%f\t%d\t%d\t",
                Radio.rxSignalStats[i].irq_count,
                (Radio.rxSignalStats[i].irq_count==0) ? 0 : double(Radio.rxSignalStats[i].rssi_sum)/Radio.rxSignalStats[i].irq_count,
                (Radio.rxSignalStats[i].irq_count==0) ? 0 : double(Radio.rxSignalStats[i].snr_sum)/Radio.rxSignalStats[i].irq_count/RADIO_SNR_SCALE,
                float(Radio.rxSignalStats[i].snr_max)/RADIO_SNR_SCALE,
                Radio.rxSignalStats[i].telem_count,
                Radio.rxSignalStats[i].fail_count);

                Radio.rxSignalStats[i].irq_count = 0;
                Radio.rxSignalStats[i].snr_sum = 0;
                Radio.rxSignalStats[i].rssi_sum = 0;
                Radio.rxSignalStats[i].snr_max = INT8_MIN;
                Radio.rxSignalStats[i].telem_count = 0;
                Radio.rxSignalStats[i].fail_count = 0;
        }
        if (isDualRadio())
        {
            DBGLN("%d\t%d", Radio.irq_count_or, Radio.irq_count_both);
        }
        else
        {
            DBGLN("");
        }
        Radio.irq_count_or = 0;
        Radio.irq_count_both = 0;

        lastReport = now;
    }
#endif
}

static void updateSwitchMode()
{
    // Negative value means waiting for confirm of the new switch mode while connected
    if (SwitchModePending <= 0)
        return;

    OtaUpdateSerializers((OtaSwitchMode_e)(SwitchModePending - 1), ExpressLRS_currAirRate_Modparams->PayloadLength);
    SwitchModePending = 0;
}

static void CheckConfigChangePending()
{
    if (config.IsModified() && !InBindingMode && connectionState < NO_CONFIG_SAVE_STATES)
    {
        LostConnection(false);
        config.Commit();
        devicesTriggerEvent();
#if defined(Regulatory_Domain_EU_CE_2400)
        LBTEnabled = (config.GetPower() > PWR_10mW);
#endif
        Radio.RXnb();
    }
}

#if defined(PLATFORM_ESP8266)
// Called from core's user_rf_pre_init() function (which is called by SDK) before setup()
RF_PRE_INIT()
{
    // Set whether the chip will do RF calibration or not when power up.
    // I believe the Arduino core fakes this (byte 114 of phy_init_data.bin)
    // to be 1, but the TX power calibration can pull over 300mA which can
    // lock up receivers built with a underspeced LDO (such as the EP2 "SDG")
    // Option 2 is just VDD33 measurement
    #if defined(RF_CAL_MODE)
    system_phy_set_powerup_option(RF_CAL_MODE);
    #else
    system_phy_set_powerup_option(2);
    #endif
}
#endif

void resetConfigAndReboot()
{
    config.SetDefaults(true);
    // Prevent WDT from rebooting too early if
    // all this flash write is taking too long
    yield();
    // Remove options.json and hardware.json
    SPIFFS.format();
    yield();
    SPIFFS.begin();
    options_SetTrueDefaults();

    ESP.restart();
}

void setup()
{
    if (!options_init())
    {
        // In the failure case we set the logging to the null logger so nothing crashes
        // if it decides to log something
        SerialLogger = new NullStream();

        // Register the WiFi with the framework
        static device_affinity_t wifi_device[] = {
            {&WIFI_device, 1}
        };
        devicesRegister(wifi_device, ARRAY_SIZE(wifi_device));
        devicesInit();

        connectionState = hardwareUndefined;
    }
    else
    {
        // default to CRSF protocol and the compiled baud rate
        serialBaud = firmwareOptions.uart_baud;

        // pre-initialise serial must be done before anything as some libs write
        // to the serial port and they'll block if the buffer fills
        #if defined(DEBUG_LOG)
        Serial.begin(serialBaud);
        SerialLogger = &Serial;
        #else
        SerialLogger = new NullStream();
        #endif

        // Init EEPROM and load config, checking powerup count
        setupConfigAndPocCheck();
        setupTarget();

        #if defined(OPT_HAS_SERVO_OUTPUT)
        // If serial is not already defined, then see if there is serial pin configured in the PWM configuration
        if (GPIO_PIN_RCSIGNAL_RX == UNDEF_PIN && GPIO_PIN_RCSIGNAL_TX == UNDEF_PIN)
        {
            for (int i = 0 ; i < GPIO_PIN_PWM_OUTPUTS_COUNT ; i++)
            {
                eServoOutputMode pinMode = (eServoOutputMode)config.GetPwmChannel(i)->val.mode;
                if (pinMode == somSerial)
                {
                    pwmSerialDefined = true;
                    break;
                }
            }
        }
        #endif
        setupSerial();
        setupSerial1();

        devicesRegister(ui_devices, ARRAY_SIZE(ui_devices));
        devicesInit();

        setupBindingFromConfig();

        FHSSrandomiseFHSSsequence(uidMacSeedGet());

        setupRadio();

        if (connectionState != radioFailed)
        {
            // RFnoiseFloor = MeasureNoiseFloor(); //TODO move MeasureNoiseFloor to driver libs
            // DBGLN("RF noise floor: %d dBm", RFnoiseFloor);

            MspReceiver.SetDataToReceive(MspData, ELRS_MSP_BUFFER);
            Radio.RXnb();
            hwTimer::init(HWtimerCallbackTick, HWtimerCallbackTock);
        }
    }

    registerButtonFunction(ACTION_BIND, EnterBindingModeSafely);
    registerButtonFunction(ACTION_RESET_REBOOT, resetConfigAndReboot);

    devicesStart();

    // setup() eats up some of this time, which can cause the first mode connection to fail.
    // Resetting the time here give the first mode a better chance of connection.
    RFmodeLastCycled = millis();
}

#if defined(PLATFORM_ESP32_C3)
void main_loop()
#else
void loop()
#endif
{
    unsigned long now = millis();

    if (MspReceiver.HasFinishedData())
    {
        MspReceiveComplete();
    }

    devicesUpdate(now);

    // read and process any data from serial ports, send any queued non-RC data
    handleSerialIO();

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
    // If the reboot time is set and the current time is past the reboot time then reboot.
    if (rebootTime != 0 && now > rebootTime) {
        ESP.restart();
    }
    #endif

    CheckConfigChangePending();
    executeDeferredFunction(micros());

    // Clear the power-on-count
    if ((connectionState == connected || connectionState == tentative) && config.GetPowerOnCounter() != 0)
    {
        config.SetPowerOnCounter(0);
    }

    if (connectionState > MODE_STATES)
    {
        return;
    }

    if ((connectionState != disconnected) && (ExpressLRS_currAirRate_Modparams->index != ExpressLRS_nextAirRateIndex)) // forced change
    {
        DBGLN("Req air rate change %u->%u", ExpressLRS_currAirRate_Modparams->index, ExpressLRS_nextAirRateIndex);
        if (!isSupportedRFRate(ExpressLRS_nextAirRateIndex))
        {
            DBGLN("Mode %u not supported, ignoring", get_elrs_airRateConfig(index)->interval);
            ExpressLRS_nextAirRateIndex = ExpressLRS_currAirRate_Modparams->index;
        }
        LostConnection(true);
        LastSyncPacket = now;           // reset this variable to stop rf mode switching and add extra time
        RFmodeLastCycled = now;         // reset this variable to stop rf mode switching and add extra time
        SendLinkStatstoFCintervalLastSent = 0;
        SendLinkStatstoFCForcedSends = 2;
    }

    if (connectionState == tentative && (now - LastSyncPacket > ExpressLRS_currAirRate_RFperfParams->RxLockTimeoutMs))
    {
        DBGLN("Bad sync, aborting");
        LostConnection(true);
        RFmodeLastCycled = now;
        LastSyncPacket = now;
    }

    cycleRfMode(now);

    uint32_t localLastValidPacket = LastValidPacket; // Required to prevent race condition due to LastValidPacket getting updated from ISR
    if ((connectionState == connected) && ((int32_t)ExpressLRS_currAirRate_RFperfParams->DisconnectTimeoutMs < (int32_t)(now - localLastValidPacket))) // check if we lost conn.
    {
        LostConnection(true);
    }

    if ((connectionState == tentative) && (abs(LPF_OffsetDx.value()) <= 10) && (LPF_Offset.value() < 100) && (LQCalc.getLQRaw() > minLqForChaos())) //detects when we are connected
    {
        GotConnection(now);
    }

    checkSendLinkStatsToFc(now);

    if ((RXtimerState == tim_tentative) && ((now - GotConnectionMillis) > ConsiderConnGoodMillis) && (abs(LPF_OffsetDx.value()) <= 5))
    {
        RXtimerState = tim_locked;
        DBGLN("Timer locked");
    }

    uint8_t *nextPayload = 0;
    uint8_t nextPlayloadSize = 0;
    if (!TelemetrySender.IsActive() && telemetry.GetNextPayload(&nextPlayloadSize, &nextPayload))
    {
        TelemetrySender.SetDataToTransmit(nextPayload, nextPlayloadSize);
    }

    uint16_t count = mavlinkInputBuffer.size();
    if (count > 0 && !TelemetrySender.IsActive())
    {
        count = std::min(count, (uint16_t)CRSF_PAYLOAD_SIZE_MAX); // Constrain to CRSF max payload size to match SS
        // First 2 bytes conform to crsf_header_s format
        mavlinkSSBuffer[0] = CRSF_ADDRESS_USB; // device_addr - used on TX to differentiate between std tlm and mavlink
        mavlinkSSBuffer[1] = count;
        // Following n bytes are just raw mavlink
        mavlinkInputBuffer.popBytes(mavlinkSSBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, count);
        nextPayload = mavlinkSSBuffer;
        nextPlayloadSize = count + CRSF_FRAME_NOT_COUNTED_BYTES;
        TelemetrySender.SetDataToTransmit(nextPayload, nextPlayloadSize);
    }

    updateTelemetryBurst();
    updateBindingMode(now);
    updateSwitchMode();
    checkGeminiMode();
    DynamicPower_UpdateRx(false);
    debugRcvrLinkstats();
    debugRcvrSignalStats(now);
}

#if defined(PLATFORM_ESP32_C3)
[[noreturn]] void loop()
{
    // We have to do this dodgy hack because on the C3 the Arduino main loop calls
    // a yield function (vTaskDelay) every 2 seconds, which causes us to lose connection!
    extern bool loopTaskWDTEnabled;
    for (;;) {
        if (loopTaskWDTEnabled) {
            esp_task_wdt_reset();
        }
        main_loop();
    }
}
#endif

struct bootloader {
    uint32_t key;
    uint32_t reset_type;
};

void reset_into_bootloader(void)
{
    SERIAL_PROTOCOL_TX.println((const char *)&target_name[4]);
    SERIAL_PROTOCOL_TX.flush();
#if defined(PLATFORM_ESP8266)
    delay(100);
    ESP.rebootIntoUartDownloadMode();
#elif defined(PLATFORM_ESP32)
    delay(100);
    connectionState = serialUpdate;
#endif
}

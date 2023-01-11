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
#include "MeanAccumulator.h"

#include "devCRSF.h"
#include "devLED.h"
#include "devLUA.h"
#include "devWIFI.h"
#include "devButton.h"
#include "devServoOutput.h"
#include "devVTXSPI.h"
#include "devAnalogVbat.h"
#include "devSerialUpdate.h"
#include "devBaro.h"

#if defined(PLATFORM_ESP8266)
#include <FS.h>
#elif defined(PLATFORM_ESP32)
#include <SPIFFS.h>
#endif

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
  {&CRSF_device, 1},
#if defined(PLATFORM_ESP32)
  {&SerialUpdate_device, 1},
#endif
#ifdef HAS_LED
  {&LED_device, 0},
#endif
  {&LUA_device, 0},
#ifdef HAS_RGB
  {&RGB_device, 0},
#endif
#ifdef HAS_WIFI
  {&WIFI_device, 0},
#endif
#ifdef HAS_BUTTON
  {&Button_device, 0},
#endif
#ifdef HAS_VTX_SPI
  {&VTxSPI_device, 0},
#endif
#ifdef USE_ANALOG_VBAT
  {&AnalogVbat_device, 0},
#endif
#ifdef HAS_SERVO_OUTPUT
  {&ServoOut_device, 1},
#endif
#ifdef HAS_BARO
  {&Baro_device, 0}, // must come after AnalogVbat_device to slow updates
#endif
};

uint8_t antenna = 0;    // which antenna is currently in use
uint8_t geminiMode = 0;

hwTimer hwTimer;
POWERMGNT POWERMGNT;
PFD PFDloop;
Crc2Byte ota_crc;
ELRS_EEPROM eeprom;
RxConfig config;
Telemetry telemetry;
Stream *SerialLogger;
bool hardwareConfigured = true;

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
unsigned long rebootTime = 0;
extern bool webserverPreventAutoStart;
#endif

/* CRSF_TX_SERIAL is used by CRSF output */
#if defined(TARGET_RX_FM30_MINI)
    HardwareSerial CRSF_TX_SERIAL(USART2);
#else
    #define CRSF_TX_SERIAL Serial
#endif
CRSF crsf(CRSF_TX_SERIAL);

/* CRSF_RX_SERIAL is used by telemetry receiver and can be on a different peripheral */
#if defined(TARGET_RX_GHOST_ATTO_V1) /* !TARGET_RX_GHOST_ATTO_V1 */
    #define CRSF_RX_SERIAL CrsfRxSerial
    HardwareSerial CrsfRxSerial(USART1, HALF_DUPLEX_ENABLED);
#elif defined(TARGET_R9SLIMPLUS_RX) /* !TARGET_R9SLIMPLUS_RX */
    #define CRSF_RX_SERIAL CrsfRxSerial
    HardwareSerial CrsfRxSerial(USART3);
#elif defined(TARGET_RX_FM30_MINI)
    #define CRSF_RX_SERIAL CRSF_TX_SERIAL
#else
    #define CRSF_RX_SERIAL Serial
#endif

StubbornSender TelemetrySender;
static uint8_t telemetryBurstCount;
static uint8_t telemetryBurstMax;

StubbornReceiver MspReceiver;
uint8_t MspData[ELRS_MSP_BUFFER];

static uint8_t NextTelemetryType = ELRS_TELEMETRY_TYPE_LINK;
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

#define LOAN_BIND_TIMEOUT_DEFAULT 60000
#define LOAN_BIND_TIMEOUT_MSP 10000U


bool InBindingMode = false;
bool InLoanBindingMode = false;
bool returnModelFromLoan = false;
static unsigned long loanBindTimeout = LOAN_BIND_TIMEOUT_DEFAULT;
static unsigned long loadBindingStartedMs = 0;

void reset_into_bootloader(void);
void EnterBindingMode();
void ExitBindingMode();
void OnELRSBindMSP(uint8_t* packet);
extern void setWifiUpdateMode();

uint8_t getLq()
{
    return LQCalc.getLQ();
}

static inline void checkGeminiMode()
{
    if (isDualRadio())
    {
        geminiMode = config.GetAntennaMode();
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
    if (GPIO_PIN_NSS_2 != UNDEF_PIN)
    {
        antenna = (Radio.GetProcessingPacketRadio() == SX12XX_Radio_1) ? 0 : 1;
    }

    int32_t rssiDBM = Radio.LastPacketRSSI;
    if (antenna == 0)
    {
        #if !defined(DEBUG_RCVR_LINKSTATS)
        rssiDBM = LPF_UplinkRSSI0.update(rssiDBM);
        #endif
        if (rssiDBM > 0) rssiDBM = 0;
        // BetaFlight/iNav expect positive values for -dBm (e.g. -80dBm -> sent as 80)
        crsf.LinkStatistics.uplink_RSSI_1 = -rssiDBM;
    }
    else
    {
        #if !defined(DEBUG_RCVR_LINKSTATS)
        rssiDBM = LPF_UplinkRSSI1.update(rssiDBM);
        #endif
        if (rssiDBM > 0) rssiDBM = 0;
        // BetaFlight/iNav expect positive values for -dBm (e.g. -80dBm -> sent as 80)
        // May be overwritten below if DEBUG_BF_LINK_STATS is set
        crsf.LinkStatistics.uplink_RSSI_2 = -rssiDBM;
    }

    // In 16ch mode, do not output RSSI/LQ on channels
    if (!SwitchModePending && (!OtaIsFullRes || OtaSwitchModeCurrent == smWideOr8ch))
    {
        crsf.ChannelData[15] = UINT10_to_CRSF(map(constrain(rssiDBM, ExpressLRS_currAirRate_RFperfParams->RXsensitivity, -50),
                                                   ExpressLRS_currAirRate_RFperfParams->RXsensitivity, -50, 0, 1023));
        crsf.ChannelData[14] = UINT10_to_CRSF(fmap(uplinkLQ, 0, 100, 0, 1023));
    }
    SnrMean.add(Radio.LastPacketSNRRaw);

    crsf.LinkStatistics.active_antenna = antenna;
    crsf.LinkStatistics.uplink_SNR = SNR_DESCALE(Radio.LastPacketSNRRaw); // possibly overriden below
    //crsf.LinkStatistics.uplink_Link_quality = uplinkLQ; // handled in Tick
    crsf.LinkStatistics.rf_Mode = ExpressLRS_currAirRate_Modparams->enum_rate;
    //DBGLN(crsf.LinkStatistics.uplink_RSSI_1);
    #if defined(DEBUG_BF_LINK_STATS)
    crsf.LinkStatistics.downlink_RSSI = debug1;
    crsf.LinkStatistics.downlink_Link_quality = debug2;
    crsf.LinkStatistics.downlink_SNR = debug3;
    crsf.LinkStatistics.uplink_RSSI_2 = debug4;
    #endif

    #if defined(DEBUG_RCVR_LINKSTATS)
    // DEBUG_RCVR_LINKSTATS gets full precision SNR, override the value
    crsf.LinkStatistics.uplink_SNR = Radio.LastPacketSNRRaw;
    debugRcvrLinkstatsFhssIdx = FHSSsequence[FHSSptr];
    #endif
}

void SetRFLinkRate(uint8_t index) // Set speed of RF link
{
    expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(index);
    expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(index);
    bool invertIQ = UID[5] & 0x01;

    uint32_t interval = ModParams->interval;
#if defined(DEBUG_FREQ_CORRECTION) && defined(RADIO_SX128X)
    interval = interval * 12 / 10; // increase the packet interval by 20% to allow adding packet header
#endif
    hwTimer.updateInterval(interval);
    Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, GetInitialFreq(),
                 ModParams->PreambleLen, invertIQ, ModParams->PayloadLength, 0
#if defined(RADIO_SX128X)
                 , uidMacSeedGet(), OtaCrcInitializer, (ModParams->radio_type == RADIO_TYPE_SX128x_FLRC)
#endif
                 );

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
        Radio.SetFrequencyReg(FHSSgetNextFreq(), SX12XX_Radio_1);
        Radio.SetFrequencyReg(FHSSgetGeminiFreq(), SX12XX_Radio_2);
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
    ls->uplink_RSSI_1 = crsf.LinkStatistics.uplink_RSSI_1;
    ls->uplink_RSSI_2 = crsf.LinkStatistics.uplink_RSSI_2;
    ls->antenna = antenna;
    ls->modelMatch = connectionHasModelMatch;
    ls->lq = crsf.LinkStatistics.uplink_Link_quality;
    ls->mspConfirm = MspReceiver.GetCurrentConfirm() ? 1 : 0;
#if defined(DEBUG_FREQ_CORRECTION)
    ls->SNR = FreqCorrection * 127 / FreqCorrectionMax;
#else
    ls->SNR = SnrMean.mean();
#endif
}

bool ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    uint8_t modresult = (OtaNonce + 1) % ExpressLRS_currTlmDenom;

    if (config.GetForceTlmOff() || (connectionState == disconnected) || (ExpressLRS_currTlmDenom == 1) || (alreadyTLMresp == true) || (modresult != 0))
    {
        return false; // don't bother sending tlm if disconnected or TLM is off
    }

#if defined(Regulatory_Domain_EU_CE_2400)
    BeginClearChannelAssessment();
#endif

    // ESP requires word aligned buffer
    WORD_ALIGNED_ATTR OTA_Packet_s otaPkt = {0};
    alreadyTLMresp = true;
    otaPkt.std.type = PACKET_TYPE_TLM;

    if (NextTelemetryType == ELRS_TELEMETRY_TYPE_LINK || !TelemetrySender.IsActive())
    {
        OTA_LinkStats_s * ls;
        if (OtaIsFullRes)
        {
            otaPkt.full.tlm_dl.containsLinkStats = 1;
            ls = &otaPkt.full.tlm_dl.ul_link_stats.stats;
            // Include some advanced telemetry in the extra space
            // Note the use of `ul_link_stats.payload` vs just `payload`
            otaPkt.full.tlm_dl.packageIndex = TelemetrySender.GetCurrentPayload(
                otaPkt.full.tlm_dl.ul_link_stats.payload,
                sizeof(otaPkt.full.tlm_dl.ul_link_stats.payload));
        }
        else
        {
            otaPkt.std.tlm_dl.type = ELRS_TELEMETRY_TYPE_LINK;
            ls = &otaPkt.std.tlm_dl.ul_link_stats.stats;
        }
        LinkStatsToOta(ls);

        NextTelemetryType = ELRS_TELEMETRY_TYPE_DATA;
        // Start the count at 1 because the next will be DATA and doing +1 before checking
        // against Max below is for some reason 10 bytes more code
        telemetryBurstCount = 1;
    }
    else
    {
        if (telemetryBurstCount < telemetryBurstMax)
        {
            telemetryBurstCount++;
        }
        else
        {
            NextTelemetryType = ELRS_TELEMETRY_TYPE_LINK;
        }

        if (OtaIsFullRes)
        {
            otaPkt.full.tlm_dl.packageIndex = TelemetrySender.GetCurrentPayload(
                otaPkt.full.tlm_dl.payload,
                sizeof(otaPkt.full.tlm_dl.payload));
        }
        else
        {
            otaPkt.std.tlm_dl.type = ELRS_TELEMETRY_TYPE_DATA;
            otaPkt.std.tlm_dl.packageIndex = TelemetrySender.GetCurrentPayload(
                otaPkt.std.tlm_dl.payload,
                sizeof(otaPkt.std.tlm_dl.payload));
        }
    }

    OtaGeneratePacketCrc(&otaPkt);

    SX12XX_Radio_Number_t transmittingRadio = geminiMode ? SX12XX_Radio_All : SX12XX_Radio_Default;
    SX12XX_Radio_Number_t clearChannelsMask = SX12XX_Radio_All;
#if defined(Regulatory_Domain_EU_CE_2400)
    clearChannelsMask = ChannelIsClear(transmittingRadio);
    if (clearChannelsMask)
#endif
    {
        Radio.TXnb((uint8_t*)&otaPkt, ExpressLRS_currAirRate_Modparams->PayloadLength, transmittingRadio & clearChannelsMask);
    }
    return true;
}

void ICACHE_RAM_ATTR HandleFreqCorr(bool value)
{
    //DBGVLN(FreqCorrection);
    if (value)
    {
        if (FreqCorrection > FreqCorrectionMin)
        {
            FreqCorrection -= 1; // FREQ_STEP units
        }
        else
        {
            DBGLN("Max -FreqCorrection reached!");
        }
    }
    else
    {
        if (FreqCorrection < FreqCorrectionMax)
        {
            FreqCorrection += 1; // FREQ_STEP units
        }
        else
        {
            DBGLN("Max +FreqCorrection reached!");
        }
    }
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
                    hwTimer.incFreqOffset();
                }
                else if (Offset < 0)
                {
                    hwTimer.decFreqOffset();
                }
            }
        }

        if (connectionState != connected)
        {
            hwTimer.phaseShift(RawOffset >> 1);
        }
        else
        {
            hwTimer.phaseShift(Offset >> 2);
        }

        DBGVLN("%d:%d:%d:%d:%d", Offset, RawOffset, OffsetDx, hwTimer.FreqOffset, uplinkLQ);
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

    crsf.LinkStatistics.uplink_Link_quality = uplinkLQ;
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

    if (ExpressLRS_currAirRate_Modparams->numOfSends > 1 && !(OtaNonce % ExpressLRS_currAirRate_Modparams->numOfSends) && LQCalcDVDA.currentIsSet())
    {
        crsfRCFrameAvailable();
        servoNewChannelsAvaliable();
    }

    if (!didFHSS) didFHSS = HandleFHSS();

    updateDiversity();
    bool tlmSent = HandleSendTelemetryResponse();

    if (!didFHSS && !tlmSent && LQCalc.currentIsSet() && Radio.FrequencyErrorAvailable())
    {
        HandleFreqCorr(Radio.GetFrequencyErrorbool());      // Adjusts FreqCorrection for RX freq offset
    #if defined(RADIO_SX127X)
        // Teamp900 also needs to adjust its demood PPM
        Radio.SetPPMoffsetReg(FreqCorrection);
    #endif /* RADIO_SX127X */
    }
    didFHSS = false;

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
    DBGLN("lost conn fc=%d fo=%d", FreqCorrection, hwTimer.FreqOffset);

    RFmodeCycleMultiplier = 1;
    connectionState = disconnected; //set lost connection
    RXtimerState = tim_disconnected;
    hwTimer.resetFreqOffset();
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
            hwTimer.stop();
        }
        SetRFLinkRate(ExpressLRS_nextAirRateIndex); // also sets to initialFreq
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

    // The caller MUST call hwTimer.resume(). It is not done here because
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

    DBGLN("got conn");
}

static void ICACHE_RAM_ATTR ProcessRfPacket_RC(OTA_Packet_s const * const otaPktPtr)
{
    // Must be fully connected to process RC packets, prevents processing RC
    // during sync, where packets can be received before connection
    if (connectionState != connected || SwitchModePending)
        return;

    bool telemetryConfirmValue = OtaUnpackChannelData(otaPktPtr, &crsf, ExpressLRS_currTlmDenom);
    TelemetrySender.ConfirmCurrentPayload(telemetryConfirmValue);

    // No channels packets to the FC or PWM pins if no model match
    if (connectionHasModelMatch)
    {
        if (ExpressLRS_currAirRate_Modparams->numOfSends == 1)
        {
            crsfRCFrameAvailable();
            servoNewChannelsAvaliable();
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
    }
    else
    {
        packageIndex = otaPktPtr->std.msp_ul.packageIndex;
        payload = otaPktPtr->std.msp_ul.payload;
        dataLen = sizeof(otaPktPtr->std.msp_ul.payload);
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
    if (connectionState != connected)
        return;

    bool currentMspConfirmValue = MspReceiver.GetCurrentConfirm();
    MspReceiver.ReceiveData(packageIndex, payload, dataLen);
    if (currentMspConfirmValue != MspReceiver.GetCurrentConfirm())
    {
        NextTelemetryType = ELRS_TELEMETRY_TYPE_LINK;
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
    // Verify the first two of three bytes of the binding ID, which should always match
    if (otaSync->UID3 != UID[3] || otaSync->UID4 != UID[4])
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

    // Will change the packet air rate in loop() if this changes
    ExpressLRS_nextAirRateIndex = otaSync->rateIndex;
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

    // don't use telemetry packets for PDF calculation since TX does not send such data and tlm frames from other rx are not in sync
    if (otaPktPtr->std.type == PACKET_TYPE_TLM)
    {
        return true;
    }

    PFDloop.extEvent(beginProcessing + PACKET_TO_TOCK_SLACK);

    bool doStartTimer = false;
    unsigned long now = millis();

    LastValidPacket = now;

    switch (otaPktPtr->std.type)
    {
    case PACKET_TYPE_RCDATA: //Standard RC Data Packet
        ProcessRfPacket_RC(otaPktPtr);
        break;
    case PACKET_TYPE_MSPDATA:
        ProcessRfPacket_MSP(otaPktPtr);
        break;
    case PACKET_TYPE_SYNC: //sync packet from master
        doStartTimer = ProcessRfPacket_SYNC(now,
            OtaIsFullRes ? &otaPktPtr->full.sync.sync : &otaPktPtr->std.sync)
            && !InBindingMode;
        break;
    default:
        break;
    }

    // Store the LQ/RSSI/Antenna
    Radio.GetLastPacketStats();
    getRFlinkInfo();
    // Received a packet, that's the definition of LQ
    LQCalc.add();
    // Extend sync duration since we've received a packet at this rate
    // but do not extend it indefinitely
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow;

#if defined(DEBUG_RX_SCOREBOARD)
    if (otaPktPtr->std.type != PACKET_TYPE_SYNC) DBGW(connectionHasModelMatch ? 'R' : 'r');
#endif
    if (doStartTimer)
        hwTimer.resume(); // will throw an interrupt immediately

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
        return true;
    }
    return false;
}

void ICACHE_RAM_ATTR TXdoneISR()
{
#if defined(Regulatory_Domain_EU_CE_2400)
    BeginClearChannelAssessment();
#else
    Radio.RXnb();
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

/**
 * Process the assembled MSP packet in MspData[]
 **/
void MspReceiveComplete()
{
    if (MspData[7] == MSP_SET_RX_CONFIG && MspData[8] == MSP_ELRS_MODEL_ID)
    {
        UpdateModelMatch(MspData[9]);
    }
    else if (MspData[0] == MSP_ELRS_SET_RX_WIFI_MODE)
    {
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
        // The MSP packet needs to be ACKed so the TX doesn't
        // keep sending it, so defer the switch to wifi
        deferExecution(500, []() {
            setWifiUpdateMode();
        });
#endif
    }
    else if (MspData[0] == MSP_ELRS_SET_RX_LOAN_MODE)
    {
        loanBindTimeout = LOAN_BIND_TIMEOUT_MSP;
        InLoanBindingMode = true;
    }
    else if (OPT_HAS_VTX_SPI && MspData[7] == MSP_SET_VTX_CONFIG)
    {
        vtxSPIBandChannelIdx = MspData[8];
        if (MspData[6] >= 4) // If packet has 4 bytes it also contains power idx and pitmode.
        {
            vtxSPIPowerIdx = MspData[10];
            vtxSPIPitmode = MspData[11];
        }
        devicesTriggerEvent();
    }
    else
    {
        crsf_ext_header_t *receivedHeader = (crsf_ext_header_t *) MspData;

        // No MSP data to the FC if no model match
        if (connectionHasModelMatch && (receivedHeader->dest_addr == CRSF_ADDRESS_BROADCAST || receivedHeader->dest_addr == CRSF_ADDRESS_FLIGHT_CONTROLLER))
        {
            crsf.sendMSPFrameToFC(MspData);
        }

        if ((receivedHeader->dest_addr == CRSF_ADDRESS_BROADCAST || receivedHeader->dest_addr == CRSF_ADDRESS_CRSF_RECEIVER))
        {
            crsf.ParameterUpdateData[0] = MspData[CRSF_TELEMETRY_TYPE_INDEX];
            crsf.ParameterUpdateData[1] = MspData[CRSF_TELEMETRY_FIELD_ID_INDEX];
            crsf.ParameterUpdateData[2] = MspData[CRSF_TELEMETRY_FIELD_CHUNK_INDEX];
            luaParamUpdateReq();
        }
    }

    MspReceiver.Unlock();
}

static void setupSerial()
{
    if (OPT_CRSF_RCVR_NO_SERIAL)
    {
        // For PWM receivers with no CRSF I/O, only turn on the Serial port if logging is on
        #if defined(DEBUG_LOG)
        Serial.begin(firmwareOptions.uart_baud);
        SerialLogger = &Serial;
        #else
        SerialLogger = new NullStream();
        #endif
        return;
    }

#ifdef PLATFORM_STM32
#if defined(TARGET_R9SLIMPLUS_RX)
    CRSF_RX_SERIAL.setRx(GPIO_PIN_RCSIGNAL_RX);
    CRSF_RX_SERIAL.begin(firmwareOptions.uart_baud);

    CRSF_TX_SERIAL.setTx(GPIO_PIN_RCSIGNAL_TX);
#else
#if defined(GPIO_PIN_RCSIGNAL_RX_SBUS) && defined(GPIO_PIN_RCSIGNAL_TX_SBUS)
    if (firmwareOptions.r9mm_mini_sbus)
    {
        CRSF_TX_SERIAL.setTx(GPIO_PIN_RCSIGNAL_TX_SBUS);
        CRSF_TX_SERIAL.setRx(GPIO_PIN_RCSIGNAL_RX_SBUS);
    }
    else
#endif
    {
        CRSF_TX_SERIAL.setTx(GPIO_PIN_RCSIGNAL_TX);
        CRSF_TX_SERIAL.setRx(GPIO_PIN_RCSIGNAL_RX);
    }
#endif /* TARGET_R9SLIMPLUS_RX */
#if defined(TARGET_RX_GHOST_ATTO_V1)
    // USART1 is used for RX (half duplex)
    CRSF_RX_SERIAL.setHalfDuplex();
    CRSF_RX_SERIAL.setTx(GPIO_PIN_RCSIGNAL_RX);
    CRSF_RX_SERIAL.begin(firmwareOptions.uart_baud);
    CRSF_RX_SERIAL.enableHalfDuplexRx();

    // USART2 is used for TX (half duplex)
    // Note: these must be set before begin()
    CRSF_TX_SERIAL.setHalfDuplex();
    CRSF_TX_SERIAL.setRx((PinName)NC);
    CRSF_TX_SERIAL.setTx(GPIO_PIN_RCSIGNAL_TX);
#endif /* TARGET_RX_GHOST_ATTO_V1 */
    CRSF_TX_SERIAL.begin(firmwareOptions.uart_baud);
#endif /* PLATFORM_STM32 */

#if defined(TARGET_RX_FM30_MINI)
    Serial.setRx(GPIO_PIN_DEBUG_RX);
    Serial.setTx(GPIO_PIN_DEBUG_TX);
    Serial.begin(firmwareOptions.uart_baud); // Same baud as CRSF for simplicity
#endif

#if defined(PLATFORM_ESP8266)
    Serial.begin(firmwareOptions.uart_baud);
    if (firmwareOptions.invert_tx)
    {
        USC0(UART0) |= BIT(UCTXI);
    }
#elif defined(PLATFORM_ESP32)
    Serial.begin(firmwareOptions.uart_baud, SERIAL_8N1, -1, -1, firmwareOptions.invert_tx);
#endif

    SerialLogger = &Serial;
}

static void setupConfigAndPocCheck()
{
    eeprom.Begin();
    config.SetStorageProvider(&eeprom); // Pass pointer to the Config class for access to storage
    config.Load();

    bool doPowerCount = config.GetOnLoan() || !firmwareOptions.hasUID;
    if (doPowerCount)
    {
        DBGLN("Doing power-up check for loan revocation and/or re-binding");
        // Increment the power on counter in eeprom
        config.SetPowerOnCounter(config.GetPowerOnCounter() + 1);
        config.Commit();

        if (config.GetPowerOnCounter() >= 3)
        {
            if (config.GetOnLoan())
            {
                config.SetOnLoan(false);
                config.SetPowerOnCounter(0);
                config.Commit();
            }
        }
        else
        {
            // We haven't reached our binding mode power cycles
            // and we've been powered on for 2s, reset the power on counter.
            // config.Commit() is done in the loop with CheckConfigChangePending().
            deferExecution(2000, []() {
                config.SetPowerOnCounter(0);
            });
        }
    }

    DBGLN("ModelId=%u", config.GetModelId());
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

#if defined(TARGET_RX_FM30_MINI)
    pinMode(GPIO_PIN_UART1TX_INVERT, OUTPUT);
    digitalWrite(GPIO_PIN_UART1TX_INVERT, LOW);
#endif

    setupTargetCommon();
}

static void setupBindingFromConfig()
{
    // Use the user defined binding phase if set,
    // otherwise use the bind flag and UID in eeprom for UID
    if (config.GetOnLoan())
    {
        DBGLN("RX has been loaned, reading the UID from eeprom...");
        memcpy(UID, config.GetOnLoanUID(), sizeof(UID));
    }
    // Check the byte that indicates if RX has been bound
    else if (!firmwareOptions.hasUID && config.GetIsBound())
    {
        DBGLN("RX has been bound previously, reading the UID from eeprom...");
        memcpy(UID, config.GetUID(), sizeof(UID));
    }

    DBGLN("UID = %d, %d, %d, %d, %d, %d", UID[0], UID[1], UID[2], UID[3], UID[4], UID[5]);
    OtaUpdateCrcInitFromUid();
}

void HandleUARTin()
{
    // If the hardware is not configured we want to be able to allow BF passthrough to work
    if (hardwareConfigured && OPT_CRSF_RCVR_NO_SERIAL)
    {
        return;
    }
    while (CRSF_RX_SERIAL.available())
    {
        telemetry.RXhandleUARTin(CRSF_RX_SERIAL.read());

        if (telemetry.ShouldCallBootloader())
        {
            reset_into_bootloader();
        }
        if (telemetry.ShouldCallEnterBind())
        {
            EnterBindingMode();
        }
        if (telemetry.ShouldCallUpdateModelMatch())
        {
            UpdateModelMatch(telemetry.GetUpdatedModelMatch());
        }
        if (telemetry.ShouldSendDeviceFrame())
        {
            uint8_t deviceInformation[DEVICE_INFORMATION_LENGTH];
            crsf.GetDeviceInformation(deviceInformation, 0);
            crsf.SetExtendedHeaderAndCrc(deviceInformation, CRSF_FRAMETYPE_DEVICE_INFO, DEVICE_INFORMATION_FRAME_SIZE, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_FLIGHT_CONTROLLER);
            crsf.sendMSPFrameToFC(deviceInformation);
        }
    }
}

static void setupRadio()
{
    Radio.currFreq = GetInitialFreq();
#if defined(RADIO_SX127X)
    //Radio.currSyncWord = UID[3];
#endif
    bool init_success = Radio.Begin();
    POWERMGNT.init();
    if (!init_success)
    {
        DBGLN("Failed to detect RF chipset!!!");
        connectionState = radioFailed;
        return;
    }

    POWERMGNT.setPower((PowerLevels_e)config.GetPower());

#if defined(Regulatory_Domain_EU_CE_2400)
    LBTEnabled = (config.GetPower() > PWR_10mW);
#endif

    Radio.RXdoneCallback = &RXdoneISR;
    Radio.TXdoneCallback = &TXdoneISR;

    scanIndex = config.GetRateInitialIdx();
    SetRFLinkRate(scanIndex);
    // Start slow on the selected rate to give it the best chance
    // to connect before beginning rate cycling
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow / 2;
}

static void updateTelemetryBurst()
{
    if (telemBurstValid)
        return;
    telemBurstValid = true;

    uint32_t hz = RateEnumToHz(ExpressLRS_currAirRate_Modparams->enum_rate);
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
        SetRFLinkRate(scanIndex % RATE_MAX); // switch between rates
        LQCalc.reset();
        LQCalcDVDA.reset();
        // Display the current air rate to the user as an indicator something is happening
        scanIndex++;
        Radio.RXnb();
        DBGLN("%u", ExpressLRS_currAirRate_Modparams->interval);

        // Switch to FAST_SYNC if not already in it (won't be if was just connected)
        RFmodeCycleMultiplier = 1;
    } // if time to switch RF mode
}

static void updateBindingMode(unsigned long now)
{
#ifndef MY_UID
    // If the eeprom is indicating that we're not bound
    // and we're not already in binding mode, enter binding
    if (!config.GetIsBound() && !InBindingMode)
    {
        INFOLN("RX has not been bound, enter binding mode...");
        EnterBindingMode();
    }
#endif
    // If in "loan" binding mode and we're not configured then start binding
    if (!InBindingMode && InLoanBindingMode && !config.GetOnLoan()) {
        EnterBindingMode();
    }
    // If in "loan" binding mode and the bind packet has come in, leave binding mode
    else if (InBindingMode && InLoanBindingMode && config.GetOnLoan()) {
        ExitBindingMode();
    }
    // If in binding mode and the bind packet has come in, leave binding mode
    else if (InBindingMode && !InLoanBindingMode && config.GetIsBound())
    {
        ExitBindingMode();
    }
    // If in "loan" binding mode and we've been here for more than timeout period, reset UID and leave binding mode
    else if (InBindingMode && InLoanBindingMode && (now - loadBindingStartedMs) > loanBindTimeout) {
        loanBindTimeout = LOAN_BIND_TIMEOUT_DEFAULT;
        memcpy(UID, MasterUID, sizeof(MasterUID));
        setupBindingFromConfig();
        ExitBindingMode();
    }
    // If returning the model to the owner, set the flag and call ExitBindingMode to reset the CRC and FHSS
    else if (returnModelFromLoan && config.GetOnLoan()) {
        LostConnection(false);
        config.SetOnLoan(false);
        memcpy(UID, MasterUID, sizeof(MasterUID));
        setupBindingFromConfig();
        ExitBindingMode();
    }

    // If the power on counter is >=3, enter binding and clear counter
    if (!firmwareOptions.hasUID && config.GetPowerOnCounter() >= 3)
    {
        config.SetPowerOnCounter(0);
        config.Commit();

        INFOLN("Power on counter >=3, enter binding mode...");
        config.SetIsBound(false);
        EnterBindingMode();
    }
}

static void checkSendLinkStatsToFc(uint32_t now)
{
    if (now - SendLinkStatstoFCintervalLastSent > SEND_LINK_STATS_TO_FC_INTERVAL)
    {
        if (connectionState == disconnected)
        {
            getRFlinkInfo();
        }

        if ((connectionState != disconnected && connectionHasModelMatch) ||
            SendLinkStatstoFCForcedSends)
        {
            crsf.sendLinkStatisticsToFC();
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
        crsfLinkStatistics_t ls = *(crsfLinkStatistics_t *)((const void *)&crsf.LinkStatistics);
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
    if (config.IsModified() && !InBindingMode && connectionState != wifiUpdate)
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
#if defined(PLATFORM_STM32)
    HAL_NVIC_SystemReset();
#else
    // Prevent WDT from rebooting too early if
    // all this flash write is taking too long
    yield();
    // Remove options.json and hardware.json
    SPIFFS.format();
    ESP.restart();
#endif
}

void setup()
{
    #if defined(TARGET_UNIFIED_RX)
    hardwareConfigured = options_init();
    if (!hardwareConfigured)
    {
        // Register the WiFi with the framework
        static device_affinity_t wifi_device[] = {
            {&WIFI_device, 1}
        };
        devicesRegister(wifi_device, ARRAY_SIZE(wifi_device));
        devicesInit();

        connectionState = hardwareUndefined;
    }
    #endif

    if (hardwareConfigured)
    {
        initUID();
        setupTarget();
        // serial setup must be done before anything as some libs write
        // to the serial port and they'll block if the buffer fills
        setupSerial();
        // Init EEPROM and load config, checking powerup count
        setupConfigAndPocCheck();

        INFOLN("ExpressLRS Module Booting...");

        devicesRegister(ui_devices, ARRAY_SIZE(ui_devices));
        devicesInit();

        setupBindingFromConfig();

        FHSSrandomiseFHSSsequence(uidMacSeedGet());

        setupRadio();

        if (connectionState != radioFailed)
        {
            // RFnoiseFloor = MeasureNoiseFloor(); //TODO move MeasureNoiseFloor to driver libs
            // DBGLN("RF noise floor: %d dBm", RFnoiseFloor);

            hwTimer.callbackTock = &HWtimerCallbackTock;
            hwTimer.callbackTick = &HWtimerCallbackTick;

            MspReceiver.SetDataToReceive(MspData, ELRS_MSP_BUFFER);
            Radio.RXnb();
            crsf.Begin();
            hwTimer.init();
        }
    }

#if defined(HAS_BUTTON)
    registerButtonFunction(ACTION_BIND, EnterBindingMode);
    registerButtonFunction(ACTION_RESET_REBOOT, resetConfigAndReboot);
#endif

    devicesStart();
}

void loop()
{
    unsigned long now = millis();

    if (MspReceiver.HasFinishedData())
    {
        MspReceiveComplete();
    }

    devicesUpdate(now);

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
    // If the reboot time is set and the current time is past the reboot time then reboot.
    if (rebootTime != 0 && now > rebootTime) {
        ESP.restart();
    }
    #endif

    CheckConfigChangePending();
    executeDeferredFunction(now);

    if (connectionState > MODE_STATES)
    {
        return;
    }

    if ((connectionState != disconnected) && (ExpressLRS_currAirRate_Modparams->index != ExpressLRS_nextAirRateIndex)){ // forced change
        DBGLN("Req air rate change %u->%u", ExpressLRS_currAirRate_Modparams->index, ExpressLRS_nextAirRateIndex);
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
    updateTelemetryBurst();
    updateBindingMode(now);
    updateSwitchMode();
    checkGeminiMode();
    debugRcvrLinkstats();
}

struct bootloader {
    uint32_t key;
    uint32_t reset_type;
};

void reset_into_bootloader(void)
{
    CRSF_TX_SERIAL.println((const char *)&target_name[4]);
    CRSF_TX_SERIAL.flush();
#if defined(PLATFORM_STM32)
    delay(100);
    DBGLN("Jumping to Bootloader...");
    delay(100);

    /** Write command for firmware update.
     *
     * Bootloader checks this memory area (if newer enough) and
     * perpare itself for fw update. Otherwise it skips the check
     * and starts ELRS firmware immediately
     */
    extern __IO uint32_t _bootloader_data;
    volatile struct bootloader * blinfo = ((struct bootloader*)&_bootloader_data) + 0;
    blinfo->key = 0x454c5253; // ELRS
    blinfo->reset_type = 0xACDC;

    HAL_NVIC_SystemReset();
#elif defined(PLATFORM_ESP8266)
    delay(100);
    ESP.rebootIntoUartDownloadMode();
#elif defined(PLATFORM_ESP32)
    delay(100);
    connectionState = serialUpdate;
#endif
}

void EnterBindingMode()
{
    if (InLoanBindingMode)
    {
        loadBindingStartedMs = millis();
        LostConnection(false);
    }
    else if (connectionState == connected || InBindingMode)
    {
        // Don't enter binding if:
        // - we're already connected
        // - we're already binding
        DBGLN("Cannot enter binding mode!");
        return;
    }

    // Set UID to special binding values
    UID[0] = BindingUID[0];
    UID[1] = BindingUID[1];
    UID[2] = BindingUID[2];
    UID[3] = BindingUID[3];
    UID[4] = BindingUID[4];
    UID[5] = BindingUID[5];

    OtaCrcInitializer = 0;
    InBindingMode = true;

    // Start attempting to bind
    // Lock the RF rate and freq while binding
    SetRFLinkRate(enumRatetoIndex(RATE_BINDING));
    Radio.SetFrequencyReg(GetInitialFreq());
    if (geminiMode)
    {
        Radio.SetFrequencyReg(FHSSgetInitialGeminiFreq(), SX12XX_Radio_2);
    }
    // If the Radio Params (including InvertIQ) parameter changed, need to restart RX to take effect
    Radio.RXnb();

    DBGLN("Entered binding mode at freq = %d", Radio.currFreq);
    devicesTriggerEvent();
}

void ExitBindingMode()
{
    if (!InBindingMode && !returnModelFromLoan)
    {
        // Not in binding mode
        DBGLN("Cannot exit binding mode, not in binding mode!");
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
    InLoanBindingMode = false;
    returnModelFromLoan = false;
    DBGLN("Exiting binding mode");
    devicesTriggerEvent();
}

void ICACHE_RAM_ATTR OnELRSBindMSP(uint8_t* newUid4)
{
    for (int i = 0; i < 4; i++)
    {
        UID[i + 2] = newUid4[i];
    }

    DBGLN("New UID = %d, %d, %d, %d, %d, %d", UID[0], UID[1], UID[2], UID[3], UID[4], UID[5]);

    // Set new UID in eeprom
    if (InLoanBindingMode)
    {
        config.SetOnLoanUID(UID);
        config.SetOnLoan(true);
    }
    else
    {
        config.SetUID(UID);
        // Set eeprom byte to indicate RX is bound
        config.SetIsBound(true);
    }

    // EEPROM commit will happen on the main thread in ExitBindingMode()
}

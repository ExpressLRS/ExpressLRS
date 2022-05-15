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

#include "devLED.h"
#include "devLUA.h"
#include "devWIFI.h"
#include "devButton.h"
#include "devVTXSPI.h"
#include "devAnalogVbat.h"

#if defined(TARGET_UNIFIED_RX)
#if defined(PLATFORM_ESP32)
#include <SPIFFS.h>
#else
#include <FS.h>
#endif
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
#ifdef HAS_LED
  {&LED_device, 1},
#endif
  {&LUA_device, 1},
#ifdef HAS_RGB
  {&RGB_device, 1},
#endif
#ifdef HAS_WIFI
  {&WIFI_device, 1},
#endif
#ifdef HAS_BUTTON
  {&Button_device, 1},
#endif
#ifdef HAS_VTX_SPI
  {&VTxSPI_device, 0},
#endif
#ifdef USE_ANALOG_VBAT
  {&AnalogVbat_device, 0},
#endif
};

uint8_t antenna = 0;    // which antenna is currently in use

hwTimer hwTimer;
POWERMGNT POWERMGNT;
PFD PFDloop;
GENERIC_CRC14 ota_crc(ELRS_CRC14_POLY);
ELRS_EEPROM eeprom;
RxConfig config;
Telemetry telemetry;
Stream *SerialLogger;
bool hardwareConfigured = true;

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
unsigned long rebootTime = 0;
extern bool webserverPreventAutoStart;
#endif

#if defined(GPIO_PIN_PWM_OUTPUTS)
#include <Servo.h>
#if defined(TARGET_UNIFIED_RX)
#if defined(PLATFORM_ESP32)
#define MAX_SERVOS 12
#endif
uint8_t SERVO_COUNT = 0;
uint8_t SERVO_PINS[MAX_SERVOS];
static Servo *Servos[MAX_SERVOS];
#else
static constexpr uint8_t SERVO_PINS[] = GPIO_PIN_PWM_OUTPUTS;
uint8_t SERVO_COUNT = ARRAY_SIZE(SERVO_PINS);
static Servo *Servos[ARRAY_SIZE(SERVO_PINS)];
#endif
#endif

static bool newChannelsAvailable = false;

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

StubbornSender TelemetrySender(ELRS_TELEMETRY_MAX_PACKAGES);
static uint8_t telemetryBurstCount;
static uint8_t telemetryBurstMax;

StubbornReceiver MspReceiver(ELRS_MSP_MAX_PACKAGES);
uint8_t MspData[ELRS_MSP_BUFFER];

static uint8_t NextTelemetryType = ELRS_TELEMETRY_TYPE_LINK;
static bool telemBurstValid;
/// Filters ////////////////
LPF LPF_Offset(2);
LPF LPF_OffsetDx(4);

// LPF LPF_UplinkRSSI(5);
LPF LPF_UplinkRSSI0(5);  // track rssi per antenna
LPF LPF_UplinkRSSI1(5);


/// LQ Calculation //////////
LQCALC<100> LQCalc;
uint8_t uplinkLQ;

uint8_t scanIndex = RATE_DEFAULT;
uint8_t ExpressLRS_nextAirRateIndex;

int32_t PfdPrevRawOffset;
RXtimerState_e RXtimerState;
uint32_t GotConnectionMillis = 0;
const uint32_t ConsiderConnGoodMillis = 1000; // minimum time before we can consider a connection to be 'good'

///////////////////////////////////////////////

volatile uint8_t NonceRX = 0; // nonce that we THINK we are up to.

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

bool InBindingMode = false;
bool InLoanBindingMode = false;
bool returnModelFromLoan = false;

void reset_into_bootloader(void);
void EnterBindingMode();
void ExitBindingMode();
void UpdateModelMatch(uint8_t model);
void OnELRSBindMSP(uint8_t* packet);

bool ICACHE_RAM_ATTR IsArmed()
{
   return CRSF_to_BIT(crsf.GetChannelOutput(AUX1));
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

    crsf.PackedRCdataOut.ch15 = UINT10_to_CRSF(map(constrain(rssiDBM, ExpressLRS_currAirRate_RFperfParams->RXsensitivity, -50),
                                               ExpressLRS_currAirRate_RFperfParams->RXsensitivity, -50, 0, 1023));
    crsf.PackedRCdataOut.ch14 = UINT10_to_CRSF(fmap(uplinkLQ, 0, 100, 0, 1023));

    crsf.LinkStatistics.active_antenna = antenna;
    crsf.LinkStatistics.uplink_SNR = Radio.LastPacketSNR;
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
                 , uidMacSeedGet(), CRCInitializer, (ModParams->radio_type == RADIO_TYPE_SX128x_FLRC)
#endif
                 );

    // Wait for (11/10) 110% of time it takes to cycle through all freqs in FHSS table (in ms)
    cycleInterval = ((uint32_t)11U * FHSSgetChannelCount() * ModParams->FHSShopInterval * interval) / (10U * 1000U);

    ExpressLRS_currAirRate_Modparams = ModParams;
    ExpressLRS_currAirRate_RFperfParams = RFperf;
    ExpressLRS_nextAirRateIndex = index; // presumably we just handled this
    telemBurstValid = false;
}

bool ICACHE_RAM_ATTR HandleFHSS()
{
    uint8_t modresultFHSS = (NonceRX + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;

    if ((ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0) || alreadyFHSS == true || InBindingMode || (modresultFHSS != 0) || (connectionState == disconnected))
    {
        return false;
    }

    alreadyFHSS = true;
    Radio.SetFrequencyReg(FHSSgetNextFreq());

    uint8_t modresultTLM = (NonceRX + 1) % (TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval));

    if (modresultTLM != 0 || ExpressLRS_currAirRate_Modparams->TLMinterval == TLM_RATIO_NO_TLM) // if we are about to send a tlm response don't bother going back to rx
    {
        Radio.RXnb();
    }
    return true;
}

bool ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;
    uint8_t modresult = (NonceRX + 1) % TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);

    if ((connectionState == disconnected) || (ExpressLRS_currAirRate_Modparams->TLMinterval == TLM_RATIO_NO_TLM) || (alreadyTLMresp == true) || (modresult != 0))
    {
        return false; // don't bother sending tlm if disconnected or TLM is off
    }

#if defined(Regulatory_Domain_EU_CE_2400)
    BeginClearChannelAssessment();
#endif

    alreadyTLMresp = true;
    Radio.TXdataBuffer[0] = TLM_PACKET;

    if (NextTelemetryType == ELRS_TELEMETRY_TYPE_LINK || !TelemetrySender.IsActive())
    {
        Radio.TXdataBuffer[1] = ELRS_TELEMETRY_TYPE_LINK;
        // The value in linkstatistics is "positivized" (inverted polarity)
        // and must be inverted on the TX side. Positive values are used
        // so save a bit to encode which antenna is in use
        Radio.TXdataBuffer[2] = crsf.LinkStatistics.uplink_RSSI_1 | (antenna << 7);
        Radio.TXdataBuffer[3] = crsf.LinkStatistics.uplink_RSSI_2 | (connectionHasModelMatch << 7);
#if defined(DEBUG_FREQ_CORRECTION)
        // Scale the FreqCorrection to +/-127
        Radio.TXdataBuffer[4] = FreqCorrection * 127 / FreqCorrectionMax;
#else
        Radio.TXdataBuffer[4] = crsf.LinkStatistics.uplink_SNR;
#endif
        Radio.TXdataBuffer[5] = crsf.LinkStatistics.uplink_Link_quality;
        Radio.TXdataBuffer[6] = MspReceiver.GetCurrentConfirm() ? 1 : 0;

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

        TelemetrySender.GetCurrentPayload(&packageIndex, &maxLength, &data);
        Radio.TXdataBuffer[1] = (packageIndex << ELRS_TELEMETRY_SHIFT) + ELRS_TELEMETRY_TYPE_DATA;
        Radio.TXdataBuffer[2] = maxLength > 0 ? *data : 0;
        Radio.TXdataBuffer[3] = maxLength >= 1 ? *(data + 1) : 0;
        Radio.TXdataBuffer[4] = maxLength >= 2 ? *(data + 2) : 0;
        Radio.TXdataBuffer[5] = maxLength >= 3 ? *(data + 3): 0;
        Radio.TXdataBuffer[6] = maxLength >= 4 ? *(data + 4): 0;
    }

    uint16_t crc = ota_crc.calc(Radio.TXdataBuffer, 7, CRCInitializer);
    Radio.TXdataBuffer[0] |= (crc >> 6) & 0b11111100;
    Radio.TXdataBuffer[7] = crc & 0xFF;

#if defined(Regulatory_Domain_EU_CE_2400)
    if (ChannelIsClear())
#endif
    {
        Radio.TXnb();
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
    if (connectionState != disconnected)
    {
        PFDloop.calcResult();
        PFDloop.reset();

        int32_t RawOffset = PFDloop.getResult();
        int32_t Offset = LPF_Offset.update(RawOffset);
        int32_t OffsetDx = LPF_OffsetDx.update(RawOffset - PfdPrevRawOffset);
        PfdPrevRawOffset = RawOffset;

        if (RXtimerState == tim_locked && LQCalc.currentIsSet())
        {
            if (NonceRX % 8 == 0) //limit rate of freq offset adjustment slightly
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
}

void ICACHE_RAM_ATTR HWtimerCallbackTick() // this is 180 out of phase with the other callback, occurs mid-packet reception
{
    updatePhaseLock();
    NonceRX++;

    // if (!alreadyTLMresp && !alreadyFHSS && !LQCalc.currentIsSet()) // packet timeout AND didn't DIDN'T just hop or send TLM
    // {
    //     Radio.RXnb(); // put the radio cleanly back into RX in case of garbage data
    // }

    // Save the LQ value before the inc() reduces it by 1
    uplinkLQ = LQCalc.getLQ();
    crsf.LinkStatistics.uplink_Link_quality = uplinkLQ;
    // Only advance the LQI period counter if we didn't send Telemetry this period
    if (!alreadyTLMresp)
        LQCalc.inc();

    alreadyTLMresp = false;
    alreadyFHSS = false;
}

//////////////////////////////////////////////////////////////
// flip to the other antenna
// no-op if GPIO_PIN_ANTENNA_SELECT not defined
static inline void switchAntenna()
{
    if (GPIO_PIN_ANTENNA_SELECT != UNDEF_PIN && config.GetAntennaMode() == 2)
    {
        // 0 and 1 is use for gpio_antenna_select
        // 2 is diversity
        antenna = !antenna;
        (antenna == 0) ? LPF_UplinkRSSI0.reset() : LPF_UplinkRSSI1.reset(); // discard the outdated value after switching
        digitalWrite(GPIO_PIN_ANTENNA_SELECT, antenna);
    }
}

static void ICACHE_RAM_ATTR updateDiversity()
{

    if (GPIO_PIN_ANTENNA_SELECT != UNDEF_PIN)
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
            digitalWrite(GPIO_PIN_ANTENNA_SELECT, config.GetAntennaMode());
            antenna = config.GetAntennaMode();
        }
    }
}

void ICACHE_RAM_ATTR HWtimerCallbackTock()
{
#if defined(Regulatory_Domain_EU_CE_2400)
    // Emulate that TX just happened, even if it didn't because channel is not clear
    if(!LBTSuccessCalc.currentIsSet())
    {
        Radio.TXdoneCallback();
    }
#endif

    PFDloop.intEvent(micros()); // our internal osc just fired

    updateDiversity();
    bool didFHSS = HandleFHSS();
    bool tlmSent = HandleSendTelemetryResponse();

    if (!didFHSS && !tlmSent && LQCalc.currentIsSet() && Radio.FrequencyErrorAvailable())
    {
        HandleFreqCorr(Radio.GetFrequencyErrorbool());      // Adjusts FreqCorrection for RX freq offset
    #if defined(RADIO_SX127X)
        // Teamp900 also needs to adjust its demood PPM
        Radio.SetPPMoffsetReg(FreqCorrection);
    #endif /* RADIO_SX127X */
    }

    #if defined(DEBUG_RX_SCOREBOARD)
    static bool lastPacketWasTelemetry = false;
    if (!LQCalc.currentIsSet() && !lastPacketWasTelemetry)
        DBGW(lastPacketCrcError ? '.' : '_');
    lastPacketCrcError = false;
    lastPacketWasTelemetry = tlmSent;
    #endif
}

void LostConnection()
{
    DBGLN("lost conn fc=%d fo=%d", FreqCorrection, hwTimer.FreqOffset);

    RFmodeCycleMultiplier = 1;
    connectionState = disconnected; //set lost connection
    RXtimerState = tim_disconnected;
    hwTimer.resetFreqOffset();
    FreqCorrection = 0;
    #if defined(RADIO_SX127X)
    Radio.SetPPMoffsetReg(0);
    #endif
    PfdPrevRawOffset = 0;
    GotConnectionMillis = 0;
    uplinkLQ = 0;
    LQCalc.reset();
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
        Radio.RXnb();
    }
}

void ICACHE_RAM_ATTR TentativeConnection(unsigned long now)
{
    PFDloop.reset();
    connectionState = tentative;
    connectionHasModelMatch = false;
    RXtimerState = tim_disconnected;
    DBGLN("tentative conn");
    FreqCorrection = 0;
    PfdPrevRawOffset = 0;
    LPF_Offset.init(0);
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

static void ICACHE_RAM_ATTR ProcessRfPacket_RC()
{
    // Must be fully connected to process RC packets, prevents processing RC
    // during sync, where packets can be received before connection
    if (connectionState != connected)
        return;

    bool telemetryConfirmValue = UnpackChannelData(Radio.RXdataBuffer, &crsf,
        NonceRX, TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval));
    TelemetrySender.ConfirmCurrentPayload(telemetryConfirmValue);

    // No channels packets to the FC or PWM pins if no model match
    if (connectionHasModelMatch)
    {
        #if defined(GPIO_PIN_PWM_OUTPUTS)
        if (SERVO_COUNT != 0)
        {
            newChannelsAvailable = true;
        }
        else
        #endif
        {
            crsf.sendRCFrameToFC();
        }
        #if defined(DEBUG_RCVR_LINKSTATS)
        debugRcvrLinkstatsPending = true;
        #endif
    }
}

/**
 * Process the assembled MSP packet in MspData[]
 **/
static void MspReceiveComplete()
{
    if (MspData[7] == MSP_SET_RX_CONFIG && MspData[8] == MSP_ELRS_MODEL_ID)
    {
        UpdateModelMatch(MspData[9]);
    }
    else if (MspData[0] == MSP_ELRS_SET_RX_WIFI_MODE)
    {
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
        connectionState = wifiUpdate;
#endif
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

static void ICACHE_RAM_ATTR ProcessRfPacket_MSP()
{
    // Always examine MSP packets for bind information if in bind mode
    // [1] is the package index, first packet of the MSP
    if (InBindingMode && Radio.RXdataBuffer[1] == 1 && Radio.RXdataBuffer[2] == MSP_ELRS_BIND)
    {
        OnELRSBindMSP((uint8_t *)&Radio.RXdataBuffer[2]);
        return;
    }

    // Must be fully connected to process MSP, prevents processing MSP
    // during sync, where packets can be received before connection
    if (connectionState != connected)
        return;

    bool currentMspConfirmValue = MspReceiver.GetCurrentConfirm();
    MspReceiver.ReceiveData(Radio.RXdataBuffer[1], Radio.RXdataBuffer + 2);
    if (currentMspConfirmValue != MspReceiver.GetCurrentConfirm())
    {
        NextTelemetryType = ELRS_TELEMETRY_TYPE_LINK;
    }
}

static bool ICACHE_RAM_ATTR ProcessRfPacket_SYNC(uint32_t now)
{
    // Verify the first two of three bytes of the binding ID, which should always match
    if (Radio.RXdataBuffer[4] != UID[3] || Radio.RXdataBuffer[5] != UID[4])
        return false;

    // The third byte will be XORed with inverse of the ModelId if ModelMatch is on
    // Only require the first 18 bits of the UID to match to establish a connection
    // but the last 6 bits must modelmatch before sending any data to the FC
    if ((Radio.RXdataBuffer[6] & ~MODELMATCH_MASK) != (UID[5] & ~MODELMATCH_MASK))
        return false;

    LastSyncPacket = now;
#if defined(DEBUG_RX_SCOREBOARD)
    DBGW('s');
#endif

    // Will change the packet air rate in loop() if this changes
    ExpressLRS_nextAirRateIndex = (Radio.RXdataBuffer[3] >> SYNC_PACKET_RATE_OFFSET) & SYNC_PACKET_RATE_MASK;
    // Update switch mode encoding immediately
    OtaSetSwitchMode((OtaSwitchMode_e)((Radio.RXdataBuffer[3] >> SYNC_PACKET_SWITCH_OFFSET) & SYNC_PACKET_SWITCH_MASK));
    // Update TLM ratio
    expresslrs_tlm_ratio_e TLMrateIn = (expresslrs_tlm_ratio_e)((Radio.RXdataBuffer[3] >> SYNC_PACKET_TLM_OFFSET) & SYNC_PACKET_TLM_MASK);
    if (ExpressLRS_currAirRate_Modparams->TLMinterval != TLMrateIn)
    {
        DBGLN("New TLMrate: %d", TLMrateIn);
        ExpressLRS_currAirRate_Modparams->TLMinterval = TLMrateIn;
        telemBurstValid = false;
    }

    // modelId = 0xff indicates modelMatch is disabled, the XOR does nothing in that case
    uint8_t modelXor = (~config.GetModelId()) & MODELMATCH_MASK;
    bool modelMatched = Radio.RXdataBuffer[6] == (UID[5] ^ modelXor);
    DBGVLN("MM %u=%u %d", Radio.RXdataBuffer[6], UID[5], modelMatched);

    if (connectionState == disconnected
        || NonceRX != Radio.RXdataBuffer[2]
        || FHSSgetCurrIndex() != Radio.RXdataBuffer[1]
        || connectionHasModelMatch != modelMatched)
    {
        //DBGLN("\r\n%ux%ux%u", NonceRX, Radio.RXdataBuffer[2], Radio.RXdataBuffer[1]);
        FHSSsetCurrIndex(Radio.RXdataBuffer[1]);
        NonceRX = Radio.RXdataBuffer[2];
        TentativeConnection(now);
        // connectionHasModelMatch must come after TentativeConnection, which resets it
        connectionHasModelMatch = modelMatched;
        return true;
    }

    return false;
}

void ICACHE_RAM_ATTR ProcessRFPacket(SX12xxDriverCommon::rx_status const status)
{
    if (status != SX12xxDriverCommon::SX12XX_RX_OK)
    {
        DBGVLN("HW CRC error");
        #if defined(DEBUG_RX_SCOREBOARD)
            lastPacketCrcError = true;
        #endif
        return;
    }
    uint32_t const beginProcessing = micros();
    uint16_t const inCRC = (((uint16_t)(Radio.RXdataBuffer[0] & 0b11111100)) << 6) | Radio.RXdataBuffer[7];
    uint8_t const type = Radio.RXdataBuffer[0] & 0b11;

    // For smHybrid the CRC only has the packet type in byte 0
    // For smHybridWide the FHSS slot is added to the CRC in byte 0 on RC_DATA_PACKETs
    if (type != RC_DATA_PACKET || OtaSwitchModeCurrent != smHybridWide)
    {
        Radio.RXdataBuffer[0] = type;
    }
    else
    {
        uint8_t NonceFHSSresult = NonceRX % ExpressLRS_currAirRate_Modparams->FHSShopInterval;
        Radio.RXdataBuffer[0] = type | (NonceFHSSresult << 2);
    }
    uint16_t calculatedCRC = ota_crc.calc(Radio.RXdataBuffer, 7, CRCInitializer);

    if (inCRC != calculatedCRC)
    {
        DBGV("CRC error: ");
        for (int i = 0; i < 8; i++)
        {
            DBGV("%x,", Radio.RXdataBuffer[i]);
        }
        DBGVCR;
        #if defined(DEBUG_RX_SCOREBOARD)
            lastPacketCrcError = true;
        #endif
        return;
    }
    PFDloop.extEvent(beginProcessing + PACKET_TO_TOCK_SLACK);

    bool doStartTimer = false;
    unsigned long now = millis();

    LastValidPacket = now;

    switch (type)
    {
    case RC_DATA_PACKET: //Standard RC Data Packet
        ProcessRfPacket_RC();
        break;
    case MSP_DATA_PACKET:
        ProcessRfPacket_MSP();
        break;
    case TLM_PACKET: //telemetry packet from master
        // not implimented yet
        break;
    case SYNC_PACKET: //sync packet from master
        doStartTimer = ProcessRfPacket_SYNC(now) && !InBindingMode;
        break;
    default: // code to be executed if n doesn't match any cases
        break;
    }

    // Store the LQ/RSSI/Antenna
    getRFlinkInfo();
    // Received a packet, that's the definition of LQ
    LQCalc.add();
    // Extend sync duration since we've received a packet at this rate
    // but do not extend it indefinitely
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow;

#if defined(DEBUG_RX_SCOREBOARD)
    if (type != SYNC_PACKET) DBGW(connectionHasModelMatch ? 'R' : 'r');
#endif
    if (doStartTimer)
        hwTimer.resume(); // will throw an interrupt immediately
}

void ICACHE_RAM_ATTR RXdoneISR(SX12xxDriverCommon::rx_status const status)
{
    ProcessRFPacket(status);
}

void ICACHE_RAM_ATTR TXdoneISR()
{
    Radio.RXnb();
#if defined(DEBUG_RX_SCOREBOARD)
    DBGW('T');
#endif
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
#else
    CRSF_TX_SERIAL.begin(firmwareOptions.uart_baud);
#endif /* TARGET_RX_GHOST_ATTO_V1 */
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
    Serial.begin(firmwareOptions.uart_baud);
    if (firmwareOptions.invert_tx)
    {
        Serial.begin(firmwareOptions.uart_baud, SERIAL_8N1, -1, -1, true);
    }
    else
    {
        Serial.begin(firmwareOptions.uart_baud, SERIAL_8N1);
    }
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
        DBGLN("Doing power-up check for loan revocation and/or re-binding")
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
            // and we've been powered on for 2s, reset the power on counter
            delay(2000);
            config.SetPowerOnCounter(0);
            config.Commit();
        }
    }

    DBGLN("ModelId=%u", config.GetModelId());
}

static void setupTarget()
{
#if defined(GPIO_PIN_ANTENNA_SELECT)
    pinMode(GPIO_PIN_ANTENNA_SELECT, OUTPUT);
    digitalWrite(GPIO_PIN_ANTENNA_SELECT, LOW);
#endif
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
        const uint8_t* storedUID = config.GetOnLoanUID();
        for (uint8_t i = 0; i < UID_LEN; ++i)
        {
            UID[i] = storedUID[i];
        }
        DBGLN("UID = %d, %d, %d, %d, %d, %d", UID[0], UID[1], UID[2], UID[3], UID[4], UID[5]);
        CRCInitializer = (UID[4] << 8) | UID[5];
        return;
    }
    // Check the byte that indicates if RX has been bound
    if (!firmwareOptions.hasUID && config.GetIsBound())
    {
        DBGLN("RX has been bound previously, reading the UID from eeprom...");
        const uint8_t* storedUID = config.GetUID();
        for (uint8_t i = 0; i < UID_LEN; ++i)
        {
            UID[i] = storedUID[i];
        }
        DBGLN("UID = %d, %d, %d, %d, %d, %d", UID[0], UID[1], UID[2], UID[3], UID[4], UID[5]);
        CRCInitializer = (UID[4] << 8) | UID[5];
    }
}

static void HandleUARTin()
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
    LBTEnabled = (MaxPower > PWR_10mW);
#endif

    Radio.RXdoneCallback = &RXdoneISR;
    Radio.TXdoneCallback = &TXdoneISR;

    SetRFLinkRate(RATE_DEFAULT);
    RFmodeCycleMultiplier = 1;
}

static void updateTelemetryBurst()
{
    if (telemBurstValid)
        return;
    telemBurstValid = true;

    uint32_t hz = RateEnumToHz(ExpressLRS_currAirRate_Modparams->enum_rate);
    uint32_t ratiodiv = TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);
    telemetryBurstMax = TLMBurstMaxForRateRatio(hz, ratiodiv);

    // Notify the sender to adjust its expected throughput
    TelemetrySender.UpdateTelemetryRate(hz, ratiodiv, telemetryBurstMax);
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
        // Display the current air rate to the user as an indicator something is happening
        scanIndex++;
        Radio.RXnb();
        INFOLN("%u", ExpressLRS_currAirRate_Modparams->interval);

        // Switch to FAST_SYNC if not already in it (won't be if was just connected)
        RFmodeCycleMultiplier = 1;
    } // if time to switch RF mode
}

static void servosUpdate(unsigned long now)
{
#if defined(GPIO_PIN_PWM_OUTPUTS)
    if (SERVO_COUNT == 0)
    {
        return;
    }
    // The ESP waveform generator is nice because it doesn't change the value
    // mid-cycle, but it does busywait if there's already a change queued.
    // Updating every 20ms minimizes the amount of waiting (0-800us cycling
    // after it syncs up) where 19ms always gets a 1000-1800us wait cycling
    static uint32_t lastUpdate;
    const uint32_t elapsed = now - lastUpdate;
    if (elapsed < 20)
        return;

    if (newChannelsAvailable)
    {
        newChannelsAvailable = false;
        for (uint8_t ch=0; ch<SERVO_COUNT; ++ch)
        {
            const rx_config_pwm_t *chConfig = config.GetPwmChannel(ch);
            uint16_t us = CRSF_to_US(crsf.GetChannelOutput(chConfig->val.inputChannel));
            if (chConfig->val.inverted)
                us = 3000U - us;

            if (Servos[ch])
                Servos[ch]->writeMicroseconds(us);
            else if (us >= 988U && us <= 2012U)
            {
                // us might be out of bounds if this is a switch channel and it has not been
                // received yet. Delay initializing the servo until the channel is valid
                Servo *servo = new Servo();
                Servos[ch] = servo;
#if defined(PLATFORM_ESP32)
                servo->attach(SERVO_PINS[ch], Servo::CHANNEL_NOT_ATTACHED, 0, 180, 988U, 2012U);
                servo->writeMicroseconds(us);
#else
                servo->attach(SERVO_PINS[ch], 988U, 2012U, us);
#endif
            }
        } /* for each servo */
    } /* if newChannelsAvailable */

    else if (elapsed > 1000U && connectionState == connected)
    {
        // No update for 1s, go to failsafe
        for (uint8_t ch=0; ch<SERVO_COUNT; ++ch)
        {
            // Note: Failsafe values do not respect the inverted flag, failsafes are absolute
            uint16_t us = config.GetPwmChannel(ch)->val.failsafe + 988U;
            if (Servos[ch])
                Servos[ch]->writeMicroseconds(us);
        }
    }

    else
        return; // prevent updating lastUpdate

    // need to sample actual millis at the end to account for any
    // waiting that happened in Servo::writeMicroseconds()
    lastUpdate = millis();
#endif
}

static void updateBindingMode()
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
    // If returning the model to the owner, set the flag and call ExitBindingMode to reset the CRC and FHSS
    else if (returnModelFromLoan && config.GetOnLoan()) {
        LostConnection();
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

void setup()
{
    #if defined(TARGET_UNIFIED_RX)
    Serial.begin(420000);
    SerialLogger = &Serial;
    SPIFFS.begin();
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
    else
    {
        #if defined(GPIO_PIN_PWM_OUTPUTS)
        SERVO_COUNT = GPIO_PIN_PWM_OUTPUTS_COUNT;
        DBGLN("%d servos");
        for (int i=0 ; i<SERVO_COUNT ; i++)
        {
            SERVO_PINS[i] = GPIO_PIN_PWM_OUTPUTS[i];
        }
        #endif
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

            MspReceiver.SetDataToReceive(ELRS_MSP_BUFFER, MspData, ELRS_MSP_BYTES_PER_CALL);
            Radio.RXnb();
            crsf.Begin();
            hwTimer.init();
        }
    }

    devicesStart();
}

void loop()
{
    unsigned long now = millis();

    HandleUARTin();
    crsf.RXhandleUARTout();

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

    if (config.IsModified() && !InBindingMode)
    {
        Radio.SetTxIdleMode();
        LostConnection();
        config.Commit();
        devicesTriggerEvent();
    }

    if (connectionState > MODE_STATES)
    {
        return;
    }

    if ((connectionState != disconnected) && (ExpressLRS_currAirRate_Modparams->index != ExpressLRS_nextAirRateIndex)){ // forced change
        DBGLN("Req air rate change %u->%u", ExpressLRS_currAirRate_Modparams->index, ExpressLRS_nextAirRateIndex);
        LostConnection();
        LastSyncPacket = now;           // reset this variable to stop rf mode switching and add extra time
        RFmodeLastCycled = now;         // reset this variable to stop rf mode switching and add extra time
        SendLinkStatstoFCintervalLastSent = 0;
        SendLinkStatstoFCForcedSends = 2;
    }

    if (connectionState == tentative && (now - LastSyncPacket > ExpressLRS_currAirRate_RFperfParams->RxLockTimeoutMs))
    {
        DBGLN("Bad sync, aborting");
        LostConnection();
        RFmodeLastCycled = now;
        LastSyncPacket = now;
    }

    cycleRfMode(now);
    if (newChannelsAvailable)
    {
        crsf.sendRCFrameToFC();
        servosUpdate(now);
        newChannelsAvailable = false;
    }

    uint32_t localLastValidPacket = LastValidPacket; // Required to prevent race condition due to LastValidPacket getting updated from ISR
    if ((connectionState == disconnectPending) ||
        ((connectionState == connected) && ((int32_t)ExpressLRS_currAirRate_RFperfParams->DisconnectTimeoutMs < (int32_t)(now - localLastValidPacket)))) // check if we lost conn.
    {
        LostConnection();
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
        TelemetrySender.SetDataToTransmit(nextPlayloadSize, nextPayload, ELRS_TELEMETRY_BYTES_PER_CALL);
    }
    updateTelemetryBurst();
    updateBindingMode();
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
#endif
}

void EnterBindingMode()
{
    if (InLoanBindingMode) {
        LostConnection();
    }
    if (connectionState == connected || InBindingMode) {
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

    CRCInitializer = 0;
    InBindingMode = true;

    // Start attempting to bind
    // Lock the RF rate and freq while binding
    SetRFLinkRate(RATE_BINDING);
    Radio.SetFrequencyReg(GetInitialFreq());
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

    // Prevent any new packets from coming in
    Radio.SetTxIdleMode();
    // Write the values to eeprom
    config.Commit();

    CRCInitializer = (UID[4] << 8) | UID[5];
    FHSSrandomiseFHSSsequence(uidMacSeedGet());

    #if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    webserverPreventAutoStart = true;
    #endif

    // Force RF cycling to start at the beginning immediately
    scanIndex = RATE_MAX;
    RFmodeLastCycled = 0;

    LostConnection();
    SetRFLinkRate(RATE_DEFAULT);
    Radio.RXnb();

    // Do this last as LostConnection() will wait for a tock that never comes
    // if we're in binding mode
    InBindingMode = false;
    InLoanBindingMode = false;
    returnModelFromLoan = false;
    DBGLN("Exiting binding mode");
    devicesTriggerEvent();
}

void ICACHE_RAM_ATTR OnELRSBindMSP(uint8_t* packet)
{
    for (int i = 1; i <=4; i++)
    {
        UID[i + 1] = packet[i];
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

void UpdateModelMatch(uint8_t model)
{
    DBGLN("Set ModelId=%u", model);

    config.SetModelId(model);
    config.Commit();
    // This will be called from ProcessRFPacket(), schedule a disconnect
    // in the main loop once the ISR has exited
    connectionState = disconnectPending;
}

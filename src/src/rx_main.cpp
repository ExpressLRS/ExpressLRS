#include "targets.h"
#include "utils.h"
#include "common.h"
#include "LowPassFilter.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
SX1280Driver Radio;
#else
#error "Radio configuration is not valid!"
#endif

#include "crc.h"
#include "CRSF.h"
#include "telemetry_protocol.h"
#include "telemetry.h"
#ifdef ENABLE_TELEMETRY
#include "stubborn_sender.h"
#endif

#include "FHSS.h"
// #include "Debug.h"
#include "OTA.h"
#include "msp.h"
#include "msptypes.h"
#include "hwTimer.h"
#include "PFD.h"
#include "LQCALC.h"
#include "elrs_eeprom.h"
#include "config.h"

#ifdef PLATFORM_ESP8266
#include "ESP8266_WebUpdate.h"
inline uint32_t esp_get_cycle_count() {
  uint32_t ccount;
  __asm__ __volatile__("rsr %0,ccount":"=a"(ccount));
  return ccount;
}
#endif

#ifdef TARGET_RX_GHOST_ATTO_V1
uint8_t LEDfadeDiv;
uint8_t LEDfade;
bool LEDfadeDir;
uint32_t LEDupdateInterval = 25;
uint32_t LEDupdateCounterMillis;
#include "STM32F3_WS2812B_LED.h"
#endif

//// CONSTANTS ////
#define BUTTON_SAMPLE_INTERVAL 150
#define WEB_UPDATE_PRESS_INTERVAL 2000 // hold button for 2 sec to enable webupdate mode
#define BUTTON_RESET_INTERVAL 4000     //hold button for 4 sec to reboot RX
#define WEB_UPDATE_LED_FLASH_INTERVAL 25
#define BIND_LED_FLASH_INTERVAL_SHORT 100
#define BIND_LED_FLASH_INTERVAL_LONG 1000
#define SEND_LINK_STATS_TO_FC_INTERVAL 100
#define DIVERSITY_ANTENNA_INTERVAL 30
#define DIVERSITY_ANTENNA_RSSI_TRIGGER 5
///////////////////

#define DEBUG_SUPPRESS // supresses debug messages on uart

uint8_t antenna = 0;    // which antenna is currently in use

hwTimer hwTimer;
PFD PFDloop; 
GENERIC_CRC8 ota_crc(ELRS_CRC_POLY);
CRSF crsf(Serial); //pass a serial port object to the class for it to use
ELRS_EEPROM eeprom;
RxConfig config;
Telemetry telemetry;

#if defined(TARGET_RX_GHOST_ATTO_V1) /* !TARGET_RX_GHOST_ATTO_V1 */
    #define CRSF_RX_SERIAL CrsfRxSerial
    HardwareSerial CrsfRxSerial(USART1, HALF_DUPLEX_ENABLED);
#elif defined(TARGET_R9SLIMPLUS_RX) /* !TARGET_R9SLIMPLUS_RX */
    #define CRSF_RX_SERIAL CrsfRxSerial
    HardwareSerial CrsfRxSerial(USART3);
#else
    #define CRSF_RX_SERIAL Serial
#endif

#ifdef ENABLE_TELEMETRY
StubbornSender TelemetrySender(ELRS_TELEMETRY_MAX_PACKAGES);
#endif



uint8_t NextTelemetryType = ELRS_TELEMETRY_TYPE_LINK;
/// Filters ////////////////
LPF LPF_Offset(2);
LPF LPF_OffsetSlow(3);
LPF LPF_OffsetDx(4);

// LPF LPF_UplinkRSSI(5);
LPF LPF_UplinkRSSI0(5);  // track rssi per antenna
LPF LPF_UplinkRSSI1(5);


/// LQ Calculation //////////
LQCALC LQCALC;
uint8_t uplinkLQ;

uint8_t scanIndex = RATE_DEFAULT;

volatile bool currentlyProcessing = false;
int32_t RawOffset;
int32_t prevRawOffset;
int32_t Offset;
int32_t OffsetSlow;
int32_t OffsetDx;
int32_t prevOffset;
RXtimerState_e RXtimerState;
uint32_t GotConnectionMillis = 0;
uint32_t ConsiderConnGoodMillis = 1000; // minimum time before we can consider a connection to be 'good'
bool lowRateMode = false;

bool LED = false;
uint8_t LEDPulseCounter = 0;

uint32_t webUpdateLedFlashIntervalLast = 0;
uint32_t bindLedFlashInterval = 0;

//// Variables Relating to Button behaviour ////
bool buttonPrevValue = true; //default pullup
bool buttonDown = false;     //is the button current pressed down?
uint32_t buttonLastSampled = 0;
uint32_t buttonLastPressed = 0;

bool webUpdateMode = false;
bool disableWebServer = false;
///////////////////////////////////////////////

volatile uint8_t NonceRX = 0; // nonce that we THINK we are up to.

bool alreadyFHSS = false;
bool alreadyTLMresp = false;

uint32_t beginProcessing;
uint32_t beginProcessingCycleCount;
uint32_t doneProcessing;

//////////////////////////////////////////////////////////////

///////Variables for Telemetry and Link Quality///////////////
uint32_t ModuleBootTime = 0;
uint32_t volatile LastValidPacketMicros = 0; // Needs to be volatile to stop race condition as variable might be updated during if() in loop()
uint32_t LastValidPacketPrevMicros = 0; //Previous to the last valid packet (used to measure the packet interval)
uint32_t LastValidPacket = 0;           //Time the last valid packet was recv
uint32_t LastSyncPacket = 0;            //Time the last valid packet was recv

uint32_t SendLinkStatstoFCintervalLastSent = 0;

int16_t RFnoiseFloor; //measurement of the current RF noise floor
///////////////////////////////////////////////////////////////

/// Variables for Sync Behaviour ////
uint32_t RFmodeLastCycled = 0;
#define RFmodeCycleDivisorFastMode 10
uint8_t RFmodeCycleDivisor = RFmodeCycleDivisorFastMode;
bool LockRFmode = false;
///////////////////////////////////////

bool InBindingMode = false;

void reset_into_bootloader(void);
void EnterBindingMode();
void ExitBindingMode();
void OnELRSBindMSP(mspPacket_t *packet);

//////////////////////////////////////////////////////////////
// flip to the other antenna
// no-op if GPIO_PIN_ANTENNA_SELECT not defined
#if defined(GPIO_PIN_ANTENNA_SELECT) && defined(USE_DIVERSITY)
    void inline switchAntenna()
    {


        antenna = !antenna;
        digitalWrite(GPIO_PIN_ANTENNA_SELECT, antenna);

    }
#endif


void ICACHE_RAM_ATTR getRFlinkInfo()
{
    //int8_t LastRSSI = Radio.LastPacketRSSI;
    // int32_t rssiDBM = LPF_UplinkRSSI.update(Radio.LastPacketRSSI);

    int32_t rssiDBM0 = LPF_UplinkRSSI0.SmoothDataINT;
    int32_t rssiDBM1 = LPF_UplinkRSSI1.SmoothDataINT;
    switch (antenna) {
        case 0:
            rssiDBM0 = LPF_UplinkRSSI0.update(Radio.LastPacketRSSI);
            break;
        case 1:
            rssiDBM1 = LPF_UplinkRSSI1.update(Radio.LastPacketRSSI);
            break;
    }

    int32_t rssiDBM = (antenna == 0) ? rssiDBM0 : rssiDBM1;


    crsf.PackedRCdataOut.ch15 = UINT10_to_CRSF(map(constrain(rssiDBM, ExpressLRS_currAirRate_RFperfParams->RXsensitivity, -50),
                                               ExpressLRS_currAirRate_RFperfParams->RXsensitivity, -50, 0, 1023));
    crsf.PackedRCdataOut.ch14 = UINT10_to_CRSF(fmap(uplinkLQ, 0, 100, 0, 1023));

    // our rssiDBM is currently in the range -128 to 98, but BF wants a value in the range
    // 0 to 255 that maps to -1 * the negative part of the rssiDBM, so cap at 0.
    // if (rssiDBM > 0)
    //     rssiDBM = 0;

    if (rssiDBM0 > 0) rssiDBM0 = 0;
    if (rssiDBM1 > 0) rssiDBM1 = 0;

    crsf.LinkStatistics.uplink_RSSI_1 = -rssiDBM0; // negate to match BF
    crsf.LinkStatistics.uplink_RSSI_2 = -rssiDBM1;
    crsf.LinkStatistics.uplink_SNR = Radio.LastPacketSNR;
    crsf.LinkStatistics.uplink_Link_quality = uplinkLQ;
    crsf.LinkStatistics.rf_Mode = (uint8_t)RATE_4HZ - (uint8_t)ExpressLRS_currAirRate_Modparams->enum_rate;
    //Serial.println(crsf.LinkStatistics.uplink_RSSI_1);
}

void SetRFLinkRate(uint8_t index) // Set speed of RF link (hz)
{
    if (InBindingMode)
    {
        return;
    }

    if (!LockRFmode)
    {
        expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(index);
        expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(index);

        Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, GetInitialFreq(), ModParams->PreambleLen, bool(DeviceAddr & 0x01));
        hwTimer.updateInterval(ModParams->interval);

        ExpressLRS_currAirRate_Modparams = ModParams;
        ExpressLRS_currAirRate_RFperfParams = RFperf;
    }
}

bool ICACHE_RAM_ATTR HandleFHSS()
{
    if ((ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0) || alreadyFHSS == true || InBindingMode)
    {
        return false;
    }

    uint8_t modresult = (NonceRX + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;

    if ((modresult != 0) || (connectionState == disconnected)) // don't hop if disconnected
    {
        return false;
    }

    alreadyFHSS = true;
    Radio.SetFrequencyReg(FHSSgetNextFreq());

    if (((NonceRX + 1) % (TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval))) != 0) // if we are about to send a tlm response don't bother going back to rx
    {
        Radio.RXnb();
    }
    return true;
}

bool ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    #ifdef ENABLE_TELEMETRY
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;
    static uint8_t telemetryDataCount = 0;
    #endif
    uint8_t openTxRSSI;

    if ((connectionState == disconnected) || (ExpressLRS_currAirRate_Modparams->TLMinterval == TLM_RATIO_NO_TLM) || (alreadyTLMresp == true))
    {
        return false; // don't bother sending tlm if disconnected or TLM is off
    }

    uint8_t modresult = (NonceRX + 1) % TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);
    if (modresult != 0)
    {
        return false;
    }

    alreadyTLMresp = true;
    Radio.TXdataBuffer[0] = (DeviceAddr << 2) + 0b11; // address + tlm packet

    switch (NextTelemetryType)
    {
        case ELRS_TELEMETRY_TYPE_LINK:
            #ifdef ENABLE_TELEMETRY
            NextTelemetryType = ELRS_TELEMETRY_TYPE_DATA;
            #else
            NextTelemetryType = ELRS_TELEMETRY_TYPE_LINK;
            #endif
            Radio.TXdataBuffer[1] = ELRS_TELEMETRY_TYPE_LINK;

            // OpenTX hard codes "rssi" warnings to the LQ sensor for crossfire, so the
            // rssi we send is for display only.
            // OpenTX treats the rssi values as signed.

            openTxRSSI = (antenna == 0) ? crsf.LinkStatistics.uplink_RSSI_1 : crsf.LinkStatistics.uplink_RSSI_2;
            // truncate the range to fit into OpenTX's 8 bit signed value
            if (openTxRSSI > 127)
                openTxRSSI = 127;
            // convert to 8 bit signed value in the negative range (-128 to 0)
            openTxRSSI = 255 - openTxRSSI;
            Radio.TXdataBuffer[2] = openTxRSSI;
            Radio.TXdataBuffer[3] = 0;
            Radio.TXdataBuffer[4] = crsf.LinkStatistics.uplink_SNR;
            Radio.TXdataBuffer[5] = crsf.LinkStatistics.uplink_Link_quality;
            Radio.TXdataBuffer[6] = 0;

            break;
        #ifdef ENABLE_TELEMETRY
        case ELRS_TELEMETRY_TYPE_DATA:
            if (ExpressLRS_currAirRate_Modparams->TLMinterval == TLM_RATIO_1_16 && telemetryDataCount < 2)
            {
                telemetryDataCount++;
            }
            else
            {
                NextTelemetryType = ELRS_TELEMETRY_TYPE_LINK;
                telemetryDataCount = 0;
            }

            TelemetrySender.GetCurrentPayload(&packageIndex, &maxLength, &data);
            Radio.TXdataBuffer[1] = (packageIndex << ELRS_TELEMETRY_SHIFT) + ELRS_TELEMETRY_TYPE_DATA;
            Radio.TXdataBuffer[2] = maxLength > 0 ? *data : 0;
            Radio.TXdataBuffer[3] = maxLength >= 1 ? *(data + 1) : 0;
            Radio.TXdataBuffer[4] = maxLength >= 2 ? *(data + 2) : 0;
            Radio.TXdataBuffer[5] = maxLength >= 3 ? *(data + 3): 0;
            Radio.TXdataBuffer[6] = maxLength >= 4 ? *(data + 4): 0;
            break;
        #endif
    }

    uint8_t crc = ota_crc.calc(Radio.TXdataBuffer, 7) + CRCCaesarCipher;
    Radio.TXdataBuffer[7] = crc;
    Radio.TXnb(Radio.TXdataBuffer, 8);
    return true;
}

void ICACHE_RAM_ATTR HandleFreqCorr(bool value)
{
    //Serial.println(FreqCorrection);
    if (!value)
    {
        if (FreqCorrection < FreqCorrectionMax)
        {
            FreqCorrection += 61; //min freq step is ~ 61hz
        }
        else
        {
            FreqCorrection = FreqCorrectionMax;
            FreqCorrection = 0; //reset because something went wrong
#ifndef DEBUG_SUPPRESS
            Serial.println("Max pos reasontable freq offset correction limit reached!");
#endif
        }
    }
    else
    {
        if (FreqCorrection > FreqCorrectionMin)
        {
            FreqCorrection -= 61; //min freq step is ~ 61hz
        }
        else
        {
            FreqCorrection = FreqCorrectionMin;
            FreqCorrection = 0; //reset because something went wrong
#ifndef DEBUG_SUPPRESS
            Serial.println("Max neg reasontable freq offset correction limit reached!");
#endif
        }
    }
}

void ICACHE_RAM_ATTR HWtimerCallbackTick() // this is 180 out of phase with the other callback
{
    if (micros() - LastValidPacketMicros < ExpressLRS_currAirRate_Modparams->interval) // only calc if we got a packet during previous reception window
    {
        PFDloop.calcResult();
    }
    PFDloop.reset();
    NonceRX++;
    alreadyFHSS = false;
    uplinkLQ = LQCALC.getLQ();
    LQCALC.inc();
    crsf.RXhandleUARTout();
}

void ICACHE_RAM_ATTR HWtimerCallbackTock()
{
    #if defined(GPIO_PIN_ANTENNA_SELECT) && defined(USE_DIVERSITY)
        static int32_t prevRSSI;        // saved rssi so that we can compare if switching made things better or worse
        static int32_t antennaLQDropTrigger;
        static int32_t antennaRSSIDropTrigger;
        int32_t rssi = (antenna == 0) ? LPF_UplinkRSSI0.SmoothDataINT : LPF_UplinkRSSI1.SmoothDataINT;
        int32_t otherRSSI = (antenna == 0) ? LPF_UplinkRSSI1.SmoothDataINT : LPF_UplinkRSSI0.SmoothDataINT;

        //if rssi dropped by the amount of DIVERSITY_ANTENNA_RSSI_TRIGGER
         if ((rssi < (prevRSSI - DIVERSITY_ANTENNA_RSSI_TRIGGER) ) && antennaRSSIDropTrigger >= DIVERSITY_ANTENNA_INTERVAL){

            switchAntenna();
            antennaLQDropTrigger = 1;
            antennaRSSIDropTrigger = 0;
         } else if(rssi > prevRSSI || antennaRSSIDropTrigger < DIVERSITY_ANTENNA_INTERVAL){
                 prevRSSI = rssi;
                 antennaRSSIDropTrigger++;
             }
        // if we didn't get a packet switch the antenna
        if (((!LQCALC.packetReceivedForPreviousFrame()) && antennaLQDropTrigger == 0)) {

            switchAntenna();
            antennaLQDropTrigger = 1;
            antennaRSSIDropTrigger = 0;
        } else if (antennaLQDropTrigger >= DIVERSITY_ANTENNA_INTERVAL) {
            // We switched antenna on the previous packet, so we now have relatively fresh rssi info for both antennas.
            // We can compare the rssi values and see if we made things better or worse when we switched

            if (rssi < otherRSSI) {
                // things got worse when we switched, so change back.

                switchAntenna();
                antennaLQDropTrigger = 1;
                antennaRSSIDropTrigger = 0;
            } else {
                // all good, we can stay on the current antenna. Clear the flag.
                antennaLQDropTrigger = 0;
            }
        } else if (antennaLQDropTrigger > 0){
            antennaLQDropTrigger ++;
        }
    #endif
    
    PFDloop.intEvent(micros()); // our internal osc just fired

    bool tlmSent = false;
    bool didFHSS = false;

    if (currentlyProcessing == false) // stop race condition 
    {
        didFHSS = HandleFHSS();
        tlmSent = HandleSendTelemetryResponse();
    }

    if (didFHSS || tlmSent)
    {
        return;
    }

    if (micros() - LastValidPacketMicros > ExpressLRS_currAirRate_Modparams->interval) // packet timeout AND didn't DIDN'T just hop or send TLM
    {
        if (connectionState == connected)
        {
            Radio.RXnb();
        }
    }
}

void LostConnection()
{
    if (connectionState == disconnected)
    {
        return; // Already disconnected
    }

    connectionStatePrev = connectionState;
    connectionState = disconnected; //set lost connection
    RXtimerState = tim_disconnected;
    FreqCorrection = 0;
    Offset = 0;
    prevOffset = 0;
    LPF_Offset.init(0);
    LPF_OffsetSlow.init(0);
    #ifdef FAST_SYNC
    RFmodeCycleDivisor = RFmodeCycleDivisorFastMode;
    #endif

    if (!InBindingMode)
    {
        Radio.SetFrequencyReg(GetInitialFreq()); // in conn lost state we always want to listen on freq index 0
        hwTimer.stop();
    }

    Serial.println("lost conn");

#ifdef GPIO_PIN_LED_GREEN
    digitalWrite(GPIO_PIN_LED_GREEN, LOW);
#endif

#ifdef GPIO_PIN_LED_RED
    digitalWrite(GPIO_PIN_LED_RED, LOW);
#endif

#ifdef GPIO_PIN_LED
    digitalWrite(GPIO_PIN_LED, 0); // turn off led
#endif
}

void ICACHE_RAM_ATTR TentativeConnection()
{
    hwTimer.resume();
    PFDloop.reset();
    connectionStatePrev = connectionState;
    connectionState = tentative;
    RXtimerState = tim_disconnected;
    Serial.println("tentative conn");
    FreqCorrection = 0;
    Offset = 0;
    prevOffset = 0;
    LPF_Offset.init(0);
    LPF_OffsetSlow.init(0);

#if WS2812_LED_IS_USED
    uint8_t LEDcolor[3] = {0};
    LEDcolor[(2 - ExpressLRS_currAirRate_Modparams->index) % 3] = 50;
    WS281BsetLED(LEDcolor);
    LEDupdateCounterMillis = millis();
#endif
}

void GotConnection()
{
    if (connectionState == connected)
    {
        return; // Already connected
    }

#ifdef LOCK_ON_FIRST_CONNECTION
    LockRFmode = true;
#endif

    connectionStatePrev = connectionState;
    connectionState = connected; //we got a packet, therefore no lost connection
    disableWebServer = true;
    RXtimerState = tim_tentative;
    GotConnectionMillis = millis();

    RFmodeLastCycled = millis(); // give another 3 sec for loc to occur.
    Serial.println("got conn");

#if WS2812_LED_IS_USED
    uint8_t LEDcolor[3] = {0};
    LEDcolor[(2 - ExpressLRS_currAirRate_Modparams->index) % 3] = 255;
    WS281BsetLED(LEDcolor);
    LEDupdateCounterMillis = millis();
#endif

#ifdef GPIO_PIN_LED_GREEN
    digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
#endif

#ifdef GPIO_PIN_LED_RED
    digitalWrite(GPIO_PIN_LED_RED, HIGH);
#endif

#ifdef GPIO_PIN_LED
    digitalWrite(GPIO_PIN_LED, HIGH); // turn on led
#endif
}

void ICACHE_RAM_ATTR ProcessRFPacket()
{
    //beginProcessingCycleCount = esp_get_cycle_count();
    beginProcessing = micros();
    uint8_t calculatedCRC = ota_crc.calc(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
    uint8_t inCRC = Radio.RXdataBuffer[7];
    uint8_t type = Radio.RXdataBuffer[0] & 0b11;
    uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;

#ifdef HYBRID_SWITCHES_8
    uint8_t SwitchEncModeExpected = 0b01;
#else
    uint8_t SwitchEncModeExpected = 0b00;
#endif
    uint8_t SwitchEncMode;
    uint8_t indexIN;
    uint8_t TLMrateIn;
    #if defined(ENABLE_TELEMETRY) && defined(HYBRID_SWITCHES_8)
    bool telemetryConfirmValue;
    #endif

    if (inCRC != calculatedCRC)
    {
        #ifndef DEBUG_SUPPRESS
        Serial.print("CRC error on RF packet: ");
        for (int i = 0; i < 8; i++)
        {
            Serial.print(Radio.RXdataBuffer[i], HEX);
            Serial.print(",");
        }
        Serial.println("");
        #endif
        return;
    }

    if (packetAddr != DeviceAddr)
    {
        #ifndef DEBUG_SUPPRESS
        Serial.println("Wrong device address on RF packet");
        #endif
        return;
    }

    currentlyProcessing = true;
    LastValidPacketPrevMicros = LastValidPacketMicros;
    LastValidPacketMicros = beginProcessing;
    LastValidPacket = millis();
    PFDloop.extEvent(beginProcessing + 250);

    #ifdef FAST_SYNC
    if(RFmodeCycleDivisor != 1){
        RFmodeCycleDivisor = 1;
    }
    #endif

    getRFlinkInfo();

    switch (type)
    {
    case RC_DATA_PACKET: //Standard RC Data Packet
        UnpackChannelData(Radio.RXdataBuffer, &crsf);
        #ifdef ENABLE_TELEMETRY
        telemetryConfirmValue = Radio.RXdataBuffer[6] & (1 << 7);
        TelemetrySender.ConfirmCurrentPayload(telemetryConfirmValue);
        #endif
        if (connectionState == connected)
        {
            crsf.sendRCFrameToFC();
        }
        break;

    case MSP_DATA_PACKET:
        mspPacket_t packet;
        UnpackMSPData(Radio.RXdataBuffer, &packet);
        if (packet.function == MSP_ELRS_BIND)
        {
            OnELRSBindMSP(&packet);
        }
        else
        {
            crsf.sendMSPFrameToFC(&packet);
        }
        break;

    case TLM_PACKET: //telemetry packet from master

        // not implimented yet
        break;

    case SYNC_PACKET: //sync packet from master
         indexIN = (Radio.RXdataBuffer[3] & 0b11000000) >> 6;
         TLMrateIn = (Radio.RXdataBuffer[3] & 0b00111000) >> 3;
         SwitchEncMode = (Radio.RXdataBuffer[3] & 0b00000110) >> 1;

         if (SwitchEncModeExpected == SwitchEncMode && ExpressLRS_currAirRate_Modparams->index == indexIN && Radio.RXdataBuffer[4] == UID[3] && Radio.RXdataBuffer[5] == UID[4] && Radio.RXdataBuffer[6] == UID[5])
         {
             LastSyncPacket = millis();
#ifndef DEBUG_SUPPRESS
             Serial.println("sync");
#endif

             if (ExpressLRS_currAirRate_Modparams->TLMinterval != (expresslrs_tlm_ratio_e)TLMrateIn)
             { // change link parameters if required
#ifndef DEBUG_SUPPRESS
                 Serial.println("New TLMrate: ");
                 Serial.println(TLMrateIn);
#endif
                 ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)TLMrateIn;
             }

             if (NonceRX != Radio.RXdataBuffer[2] || FHSSgetCurrIndex() != Radio.RXdataBuffer[1])
             {
                 FHSSsetCurrIndex(Radio.RXdataBuffer[1]);
                 NonceRX = Radio.RXdataBuffer[2];
                 TentativeConnection();
                 return;
             }
         }
         break;

    default: // code to be executed if n doesn't match any cases
        break;
    }

    HandleFHSS();
    HandleSendTelemetryResponse();
    LQCALC.add(); // Adds packet to LQ calculation otherwise an artificial drop in LQ is seen due to sending TLM.

    if (connectionState != disconnected)
    {
        RawOffset = PFDloop.getResult();
        Offset = LPF_Offset.update(RawOffset);
        OffsetSlow = LPF_OffsetSlow.update(RawOffset);
        OffsetDx = abs(LPF_OffsetDx.update(RawOffset - prevOffset));

        if (connectionState != connected)
        {
            hwTimer.phaseShift((RawOffset >> 1));
        }
        else
        {
            hwTimer.phaseShift((Offset >> 2));
        }

        if (RXtimerState == tim_locked) 
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
        else
        {
            hwTimer.phaseShift((RawOffset >> 1));
        }
        prevOffset = Offset;
        prevRawOffset = RawOffset;
    }

#if !defined(Regulatory_Domain_ISM_2400)
    if ((alreadyFHSS == false) || (ExpressLRS_currAirRate_Modparams->index > 2))
    {
        HandleFreqCorr(Radio.GetFrequencyErrorbool()); //corrects for RX freq offset
        Radio.SetPPMoffsetReg(FreqCorrection);         //as above but corrects a different PPM offset based on freq error
    }
#endif /* Regulatory_Domain_ISM_2400 */

    doneProcessing = micros();

#ifndef DEBUG_SUPPRESS
    Serial.print(Offset);
    Serial.print(":");
    Serial.print(RawOffset);
    Serial.print(":");
    Serial.print(OffsetDx);
    Serial.print(":");
    Serial.print(hwTimer.FreqOffset);
    Serial.print(":");
    Serial.println(uplinkLQ);
#endif
    currentlyProcessing = false;
}

void beginWebsever()
{
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
    hwTimer.stop();
    BeginWebUpdate();
    webUpdateMode = true;
#endif
}

void sampleButton()
{
#ifdef GPIO_PIN_BUTTON
    bool buttonValue = digitalRead(GPIO_PIN_BUTTON);

    if (buttonValue == false && buttonPrevValue == true)
    { //falling edge
        buttonDown = true;
    }

    if (buttonValue == true && buttonPrevValue == false)
    { //rising edge
        buttonDown = false;
    }

    if ((millis() > buttonLastPressed + WEB_UPDATE_PRESS_INTERVAL) && buttonDown)
    { // button held down for WEB_UPDATE_PRESS_INTERVAL
        if (!webUpdateMode)
        {
            beginWebsever();
        }
    }
    if ((millis() > buttonLastPressed + BUTTON_RESET_INTERVAL) && buttonDown)
    {
#ifdef PLATFORM_ESP8266
        ESP.restart();
#endif
    }
    buttonPrevValue = buttonValue;
#endif
}

void ICACHE_RAM_ATTR RXdoneISR()
{
    ProcessRFPacket();
    //Serial.println("r");
}

void ICACHE_RAM_ATTR TXdoneISR()
{
    alreadyTLMresp = false;
    LQCALC.add(); // Adds packet to LQ calculation otherwise an artificial drop in LQ is seen due to sending TLM.
    Radio.RXnb();
}

void setup()
{
#ifdef PLATFORM_STM32
#if defined(TARGET_R9SLIMPLUS_RX)
    CRSF_RX_SERIAL.setRx(GPIO_PIN_RCSIGNAL_RX);
    CRSF_RX_SERIAL.begin(CRSF_RX_BAUDRATE);

    Serial.setTx(GPIO_PIN_RCSIGNAL_TX);
#else /* !TARGET_R9SLIMPLUS_RX */
    Serial.setTx(GPIO_PIN_RCSIGNAL_TX);
    Serial.setRx(GPIO_PIN_RCSIGNAL_RX);
#endif /* TARGET_R9SLIMPLUS_RX */
#if defined(TARGET_RX_GHOST_ATTO_V1)
    // USART1 is used for RX (half duplex)
    CRSF_RX_SERIAL.setHalfDuplex();
    CRSF_RX_SERIAL.setTx(GPIO_PIN_RCSIGNAL_RX);
    CRSF_RX_SERIAL.begin(CRSF_RX_BAUDRATE);
    CRSF_RX_SERIAL.enableHalfDuplexRx();

    // USART2 is used for TX (half duplex)
    // Note: these must be set before begin()
    Serial.setHalfDuplex();
    Serial.setRx((PinName)NC);
    Serial.setTx(GPIO_PIN_RCSIGNAL_TX);
#endif /* TARGET_RX_GHOST_ATTO_V1 */
#endif /* PLATFORM_STM32 */

    Serial.begin(CRSF_RX_BAUDRATE);

    Serial.println("ExpressLRS Module Booting...");

    eeprom.Begin();
    config.Load();

#ifndef MY_UID
    // Increment the power on counter in eeprom
    config.SetPowerOnCounter(config.GetPowerOnCounter() + 1);
    config.Commit();

    // If we haven't reached our binding mode power cycles
    // and we've been powered on for 2s, reset the power on counter
    if (config.GetPowerOnCounter() < 3)
    {
        delay(2000);
        config.SetPowerOnCounter(0);
        config.Commit();
    }
#endif

#ifdef PLATFORM_ESP8266
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
#endif /* PLATFORM_ESP8266 */

#ifdef GPIO_PIN_LED_GREEN
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
#endif /* GPIO_PIN_LED_GREEN */
#ifdef GPIO_PIN_LED_RED
    pinMode(GPIO_PIN_LED_RED, OUTPUT);
#endif /* GPIO_PIN_LED_RED */
#if defined(GPIO_PIN_LED)
    pinMode(GPIO_PIN_LED, OUTPUT);
#endif /* GPIO_PIN_LED */
#ifdef GPIO_PIN_BUTTON
    pinMode(GPIO_PIN_BUTTON, INPUT);
#endif /* GPIO_PIN_BUTTON */

#if WS2812_LED_IS_USED // do startup blinkies for fun
    WS281Binit();
    uint32_t col = 0x0000FF;
    for (uint8_t j = 0; j < 3; j++)
    {
        for (uint8_t i = 0; i < 5; i++)
        {
            WS281BsetLED(col << j*8);
            delay(15);
            WS281BsetLED(0, 0, 0);
            delay(35);
        }
    }
#endif

#if defined(GPIO_PIN_ANTENNA_SELECT)
  pinMode(GPIO_PIN_ANTENNA_SELECT, OUTPUT);
  digitalWrite(GPIO_PIN_ANTENNA_SELECT, LOW);
#endif
#ifdef Regulatory_Domain_AU_915
    Serial.println("Setting 915MHz Mode");
#elif defined Regulatory_Domain_FCC_915
    Serial.println("Setting 915MHz Mode");
#elif defined Regulatory_Domain_EU_868
    Serial.println("Setting 868MHz Mode");
#elif defined Regulatory_Domain_AU_433 || defined Regulatory_Domain_EU_433
    Serial.println("Setting 433MHz Mode");
#elif defined Regulatory_Domain_ISM_2400
    Serial.println("Setting 2.4GHz Mode");
#endif

// Use the user defined binding phase if set,
// otherwise use the bind flag and UID in eeprom for UID
#if !defined(MY_UID)
    // Check the byte that indicates if RX has been bound
    if (config.GetIsBound())
    {
        Serial.println("RX has been bound previously, reading the UID from eeprom...");
        uint8_t* storedUID = config.GetUID();
        for (uint8_t i = 0; i < UID_LEN; ++i)
        {
            UID[i] = storedUID[i];
        }

        CRCCaesarCipher = UID[4];
        DeviceAddr = UID[5] & 0b111111;

        Serial.print("UID = ");
        Serial.print(UID[0]);
        Serial.print(", ");
        Serial.print(UID[1]);
        Serial.print(", ");
        Serial.print(UID[2]);
        Serial.print(", ");
        Serial.print(UID[3]);
        Serial.print(", ");
        Serial.print(UID[4]);
        Serial.print(", ");
        Serial.println(UID[5]);
    }
#endif

    FHSSrandomiseFHSSsequence();

    Radio.currFreq = GetInitialFreq();
#if !defined(Regulatory_Domain_ISM_2400)
    //Radio.currSyncWord = UID[3];
#endif
    bool init_success = Radio.Begin();
    while (!init_success)
    {
        #ifdef GPIO_PIN_LED
        digitalWrite(GPIO_PIN_LED, LED);
        #endif
        LED = !LED;
        delay(200);
        Serial.println("Failed to detect RF chipset!!!");
    }
#ifdef TARGET_SX1280
    Radio.SetOutputPower(13); //default is max power (12.5dBm for SX1280 RX)
#else
    Radio.SetOutputPower(0b1111); //default is max power (17dBm for SX127x RX@)
#endif

    // RFnoiseFloor = MeasureNoiseFloor(); //TODO move MeasureNoiseFloor to driver libs
    // Serial.print("RF noise floor: ");
    // Serial.print(RFnoiseFloor);
    // Serial.println("dBm");

    Radio.RXdoneCallback = &RXdoneISR;
    Radio.TXdoneCallback = &TXdoneISR;

    hwTimer.callbackTock = &HWtimerCallbackTock;
    hwTimer.callbackTick = &HWtimerCallbackTick;

    SetRFLinkRate(RATE_DEFAULT);

    telemetry.ResetState();
    #ifdef ENABLE_TELEMETRY
    TelemetrySender.ResetState();
    #endif
    Radio.RXnb();
    crsf.Begin();
    hwTimer.init();
    hwTimer.stop();
    LQCALC.init();
}

void loop()
{
    #ifdef ENABLE_TELEMETRY
    uint8_t *nextPayload = 0;
    uint8_t nextPlayloadSize = 0;
    #endif

    if (hwTimer.running == false)
    {
        crsf.RXhandleUARTout();
    }

    #if defined(PLATFORM_ESP8266) && defined(AUTO_WIFI_ON_INTERVAL)
    if (!disableWebServer && (connectionState == disconnected) && !webUpdateMode && !InBindingMode && millis() > (AUTO_WIFI_ON_INTERVAL*1000))
    {
        beginWebsever();
    }

    if (!disableWebServer && (connectionState == disconnected) && !webUpdateMode && InBindingMode && millis() > 60000)
    {
        beginWebsever();
    }

    if (webUpdateMode)
    {
        HandleWebUpdate();
        if (millis() > WEB_UPDATE_LED_FLASH_INTERVAL + webUpdateLedFlashIntervalLast)
        {
            #ifdef GPIO_PIN_LED
            digitalWrite(GPIO_PIN_LED, LED);
            #endif
            LED = !LED;
            webUpdateLedFlashIntervalLast = millis();
        }
        return;
    }
    #endif

    if (connectionState == tentative && (uplinkLQ <= (100-(100/ExpressLRS_currAirRate_Modparams->FHSShopInterval)) || abs(OffsetDx) > 10 || Offset > 100) && (millis() > (LastSyncPacket + ExpressLRS_currAirRate_RFperfParams->RFmodeCycleAddtionalTime)))
    {
        LostConnection();
        Serial.println("Bad sync, aborting");
        Radio.SetFrequencyReg(GetInitialFreq());
        Radio.RXnb();
        RFmodeLastCycled = millis();
        LastSyncPacket = millis();
    }

#ifdef FAST_SYNC
    if (millis() > (RFmodeLastCycled + (ExpressLRS_currAirRate_RFperfParams->RFmodeCycleInterval/RFmodeCycleDivisor)))
#else
        if (millis() > (RFmodeLastCycled + (ExpressLRS_currAirRate_RFperfParams->RFmodeCycleInterval)))
#endif
    {
        if ((connectionState == disconnected) && !webUpdateMode)
        {
            LastSyncPacket = millis();           // reset this variable
            SetRFLinkRate(scanIndex % RATE_MAX); // switch between rates
            SendLinkStatstoFCintervalLastSent = millis();
            LQCALC.reset();
            Serial.println(ExpressLRS_currAirRate_Modparams->interval);
            scanIndex++;
            getRFlinkInfo();
            crsf.sendLinkStatisticsToFC();
            delay(100);
            crsf.sendLinkStatisticsToFC(); // need to send twice, not sure why, seems like a BF bug?
            Radio.RXnb();
            if (!InBindingMode)
            {
                #ifdef GPIO_PIN_LED
                    digitalWrite(GPIO_PIN_LED, LED);
                #elif GPIO_PIN_LED_GREEN
                    digitalWrite(GPIO_PIN_LED_GREEN, LED);
                #endif
                LED = !LED;
            }
        }
        RFmodeLastCycled = millis();
    }

    uint32_t localLastValidPacket = LastValidPacket; // Required to prevent race condition due to LastValidPacket getting updated from ISR
    if ((connectionState == connected) && ((int32_t)ExpressLRS_currAirRate_RFperfParams->RFmodeCycleInterval < (int32_t)(millis() - localLastValidPacket))) // check if we lost conn.
    {
        LostConnection();
    }

    if ((connectionState == tentative) && (abs(OffsetDx) <= 10) && (uplinkLQ > (100 - (100 / (ExpressLRS_currAirRate_Modparams->FHSShopInterval + 1))))) //detects when we are connected
    {
        GotConnection();
    }

    if (millis() > (SendLinkStatstoFCintervalLastSent + SEND_LINK_STATS_TO_FC_INTERVAL))
    {
        if (connectionState != disconnected)
        {
            getRFlinkInfo();
            crsf.sendLinkStatisticsToFC();
            SendLinkStatstoFCintervalLastSent = millis();
        }
    }

    if (millis() > (buttonLastSampled + BUTTON_SAMPLE_INTERVAL))
    {
        sampleButton();
        buttonLastSampled = millis();
    }

    if ((RXtimerState == tim_tentative) && (millis() > (GotConnectionMillis + ConsiderConnGoodMillis)) && (OffsetDx <= 5))
    {
        RXtimerState = tim_locked;
        #ifndef DEBUG_SUPPRESS
        Serial.println("Timer Considered Locked");
        #endif
    }

#if WS2812_LED_IS_USED
    if ((connectionState == disconnected) && (millis() > LEDupdateInterval + LEDupdateCounterMillis))
    {
        uint8_t LEDcolor[3] = {0};
        if (LEDfade == 30 || LEDfade == 0)
        {
            LEDfadeDir = !LEDfadeDir;
        }

        LEDfadeDir ? LEDfade = LEDfade + 2 :  LEDfade = LEDfade - 2;
        LEDcolor[(2 - ExpressLRS_currAirRate_Modparams->index) % 3] = LEDfade;
        WS281BsetLED(LEDcolor);
        LEDupdateCounterMillis = millis();
    }
#endif

    // If the eeprom is indicating that we're not bound
    // and we're not already in binding mode, enter binding
    if (!config.GetIsBound() && !InBindingMode)
    {
        Serial.println("RX has not been bound, enter binding mode...");
        EnterBindingMode();
    }

    // If the power on counter is >=3, enter binding and clear counter
#ifndef MY_UID
    if (config.GetPowerOnCounter() >= 3)
    {
        config.SetPowerOnCounter(0);
        config.Commit();

        Serial.println("Power on counter >=3, enter binding mode...");
        EnterBindingMode();
    }
#endif
    // Update the LED while in binding mode
    if (InBindingMode)
    {
        if (millis() > bindLedFlashInterval)
        {
            if (LEDPulseCounter == 0)
            {
                LED = true;
            }
            else if (LEDPulseCounter == 4)
            {
                LED = false;
            }
            else
            {
                LED = !LED;
            }

            if (LEDPulseCounter < 4)
            {
                bindLedFlashInterval = millis() + BIND_LED_FLASH_INTERVAL_SHORT;
            }
            else
            {
                bindLedFlashInterval = millis() + BIND_LED_FLASH_INTERVAL_LONG;
                LEDPulseCounter = 0;
            }


            #ifdef GPIO_PIN_LED
            digitalWrite(GPIO_PIN_LED, LED);
            #endif

            LEDPulseCounter++;
        }
    }


    while (CRSF_RX_SERIAL.available())
    {
        telemetry.RXhandleUARTin(CRSF_RX_SERIAL.read());

        if (telemetry.ShouldCallBootloader())
        {
            reset_into_bootloader();
        }

        #ifdef ENABLE_TELEMETRY
        if (!TelemetrySender.IsActive() && telemetry.GetNextPayload(&nextPlayloadSize, &nextPayload))
        {
            TelemetrySender.SetDataToTransmit(nextPlayloadSize, nextPayload, ELRS_TELEMETRY_BYTES_PER_CALL);
        }
        #endif
    }
}

struct bootloader {
    uint32_t key;
    uint32_t reset_type;
};

void reset_into_bootloader(void)
{
#if defined(PLATFORM_STM32)
    delay(100);
    Serial.println("Jumping to Bootloader...");
    delay(100);

#if BOOTLOADER_DATA_EXCHANGE_ENABLED
    extern __IO uint32_t _bootloader_data;
    volatile struct bootloader * blinfo = ((struct bootloader*)&_bootloader_data) + 0;
    blinfo->key = 0x454c5253; // ELRS
    blinfo->reset_type = 0xACDC;
#endif /* BOOTLOADER_DATA_EXCHANGE_ENABLED */

    HAL_NVIC_SystemReset();
#endif /* PLATFORM_STM32 */
}

void EnterBindingMode()
{
    if ((connectionState == connected) || InBindingMode || webUpdateMode) {
        // Don't enter binding if:
        // - we're already connected
        // - we're already binding
        // - we're in web update mode
        Serial.println("Cannot enter binding mode!");
        return;
    }

    // Set UID to special binding values
    UID[0] = BindingUID[0];
    UID[1] = BindingUID[1];
    UID[2] = BindingUID[2];
    UID[3] = BindingUID[3];
    UID[4] = BindingUID[4];
    UID[5] = BindingUID[5];

    CRCCaesarCipher = UID[4];
    DeviceAddr = UID[5] & 0b111111;

    // Start attempting to bind
    // Lock the RF rate and freq while binding
    SetRFLinkRate(RATE_DEFAULT);
    Radio.SetFrequencyReg(GetInitialFreq());

    InBindingMode = true;

    Serial.print("Entered binding mode at freq = ");
    Serial.println(Radio.currFreq);
}

void ExitBindingMode()
{
    if (!InBindingMode) {
        // Not in binding mode
        Serial.println("Cannot exit binding mode, not in binding mode!");
        return;
    }

    InBindingMode = false;

    LostConnection();

    // Sets params to allow sync after binding and not instantly change RF mode
    RFmodeCycleDivisor = 1;
    RFmodeLastCycled = millis();

    Serial.print("Exit binding mode at freq = ");
    Serial.println(Radio.currFreq);
}

void OnELRSBindMSP(mspPacket_t *packet)
{
    UID[2] = packet->readByte();
    UID[3] = packet->readByte();
    UID[4] = packet->readByte();
    UID[5] = packet->readByte();
    CRCCaesarCipher = UID[4];
    DeviceAddr = UID[5] & 0b111111;

    Serial.print("New UID = ");
    Serial.print(UID[0]);
    Serial.print(", ");
    Serial.print(UID[1]);
    Serial.print(", ");
    Serial.print(UID[2]);
    Serial.print(", ");
    Serial.print(UID[3]);
    Serial.print(", ");
    Serial.print(UID[4]);
    Serial.print(", ");
    Serial.println(UID[5]);

    // Set new UID in eeprom
    config.SetUID(UID);

    // Set eeprom byte to indicate RX is bound
    config.SetIsBound(true);

    // Write the values to eeprom
    config.Commit();

    FHSSrandomiseFHSSsequence();

    disableWebServer = true;
    ExitBindingMode();
}

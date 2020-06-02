#include <Arduino.h>
#include "targets.h"
#include "utils.h"
#include "common.h"
#include "LowPassFilter.h"
#include "SX127xDriver.h"
#include "CRSF.h"
#include "FHSS.h"
// #include "Debug.h"
#include "rx_LinkQuality.h"
#include "OTA.h"
#include "msp.h"
#include "msptypes.h"

#ifdef PLATFORM_ESP8266
#include "ESP8266_WebUpdate.h"
#include "ESP8266_hwTimer.h"
#endif

#ifdef PLATFORM_STM32
#include "STM32_UARTinHandler.h"
#include "STM32_hwTimer.h"
#endif

//// CONSTANTS ////
#define BUTTON_SAMPLE_INTERVAL 150
#define WEB_UPDATE_PRESS_INTERVAL 2000 // hold button for 2 sec to enable webupdate mode
#define BUTTON_RESET_INTERVAL 4000     //hold button for 4 sec to reboot RX
#define WEB_UPDATE_LED_FLASH_INTERVAL 25
#define SEND_LINK_STATS_TO_FC_INTERVAL 100
///////////////////

//#define DEBUG_SUPPRESS // supresses debug messages on uart

hwTimer hwTimer;
SX127xDriver Radio;
CRSF crsf(Serial); //pass a serial port object to the class for it to use

/// Filters ////////////////
LPF LPF_PacketInterval(3);
LPF LPF_Offset(2);
LPF LPF_OffsetDx(4);
LPF LPF_UplinkRSSI(5);
////////////////////////////

uint8_t scanIndex = 0;
uint8_t CURR_RATE_MAX;

int32_t HWtimerError;
int32_t RawOffset;
int32_t Offset;
int32_t OffsetDx;
int32_t prevOffset;
RXtimerState_e RXtimerState;
uint32_t GotConnectionMillis = 0;
uint32_t ConsiderConnGoodMillis = 1000; // minimum time before we can consider a connection to be 'good'

bool LED = false;

//// Variables Relating to Button behaviour ////
bool buttonPrevValue = true; //default pullup
bool buttonDown = false;     //is the button current pressed down?
uint32_t buttonLastSampled = 0;
uint32_t buttonLastPressed = 0;

bool webUpdateMode = false;
uint32_t webUpdateLedFlashIntervalLast;
///////////////////////////////////////////////

volatile uint8_t NonceRX = 0; // nonce that we THINK we are up to.

bool alreadyFHSS = false;
bool alreadyTLMresp = false;

uint32_t headroom;
uint32_t headroom2;
uint32_t beginProcessing;
uint32_t doneProcessing;

//////////////////////////////////////////////////////////////

///////Variables for Telemetry and Link Quality///////////////
uint32_t ModuleBootTime = 0;
uint32_t LastValidPacketMicros = 0;
uint32_t LastValidPacketPrevMicros = 0; //Previous to the last valid packet (used to measure the packet interval)
uint32_t LastValidPacket = 0;           //Time the last valid packet was recv
uint32_t LastSyncPacket = 0;            //Time the last valid packet was recv

uint32_t SendLinkStatstoFCintervalLastSent = 0;

int16_t RFnoiseFloor; //measurement of the current RF noise floor
///////////////////////////////////////////////////////////////

uint32_t PacketIntervalError;
uint32_t PacketInterval;

/// Variables for Sync Behaviour ////
uint32_t RFmodeLastCycled = 0;
///////////////////////////////////////

void ICACHE_RAM_ATTR getRFlinkInfo()
{
    int8_t LastRSSI = Radio.GetLastPacketRSSI();
    crsf.PackedRCdataOut.ch15 = UINT10_to_CRSF(map(LastRSSI, -100, -50, 0, 1023));
    crsf.PackedRCdataOut.ch14 = UINT10_to_CRSF(fmap(linkQuality, 0, 100, 0, 1023));

    int32_t rssiDBM = LPF_UplinkRSSI.update(Radio.LastPacketRSSI);
    // our rssiDBM is currently in the range -128 to 98, but BF wants a value in the range
    // 0 to 255 that maps to -1 * the negative part of the rssiDBM, so cap at 0.
    if (rssiDBM > 0)
        rssiDBM = 0;
    crsf.LinkStatistics.uplink_RSSI_1 = -1 * rssiDBM; // to match BF

    crsf.LinkStatistics.uplink_RSSI_2 = 0;
    crsf.LinkStatistics.uplink_SNR = Radio.LastPacketSNR * 10;
    crsf.LinkStatistics.uplink_Link_quality = linkQuality;
    crsf.LinkStatistics.rf_Mode = 4 - ExpressLRS_currAirRate_Modparams->enum_rate;

    //Serial.println(crsf.LinkStatistics.uplink_RSSI_1);
}

void ICACHE_RAM_ATTR SetRFLinkRate(expresslrs_RFrates_e rate) // Set speed of RF link (hz)
{
    expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(rate);
    expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(rate);

    Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, ModParams->PreambleLen);
    hwTimer.updateInterval(ModParams->interval);

    ExpressLRS_currAirRate_Modparams = ModParams;
    ExpressLRS_currAirRate_RFperfParams = RFperf;

    Radio.RXnb();
}

void ICACHE_RAM_ATTR SetTLMRate(expresslrs_tlm_ratio_e TLMrateIn)
{
    ExpressLRS_currAirRate_Modparams->TLMinterval = TLMrateIn;
}

void ICACHE_RAM_ATTR HandleFHSS()
{
    if ((ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0) || alreadyFHSS == true)
    {
        return;
    }

    uint8_t modresult = (NonceRX + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;

    if ((modresult != 0) || (connectionState == disconnected)) // don't hop if disconnected
    {
        return;
    }

    alreadyFHSS = true;
    Radio.SetFrequency(FHSSgetNextFreq());

    if (ExpressLRS_currAirRate_Modparams->TLMinterval == TLM_RATIO_NO_TLM)
    {
        Radio.RXnb();
        return;
    }
    else if (((NonceRX + 1) % (TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval))) != 0) // if we aren't about to send a response don't go back into RX mode
    {
        Radio.RXnb();
        return;
    }
}

void ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    if ((connectionState == disconnected) || (ExpressLRS_currAirRate_Modparams->TLMinterval == TLM_RATIO_NO_TLM) || (alreadyTLMresp == true))
    {
        return; // don't bother sending tlm if disconnected or TLM is off
    }

    uint8_t modresult = (NonceRX + 1) % TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);
    if (modresult != 0)
    {
        return;
    }

    alreadyTLMresp = true;

    Radio.TXdataBuffer[0] = (DeviceAddr << 2) + 0b11; // address + tlm packet
    Radio.TXdataBuffer[1] = CRSF_FRAMETYPE_LINK_STATISTICS;

    // OpenTX hard codes "rssi" warnings to the LQ sensor for crossfire, so the
    // rssi we send is for display only.
    // OpenTX treats the rssi values as signed.

    uint8_t openTxRSSI = crsf.LinkStatistics.uplink_RSSI_1;
    // truncate the range to fit into OpenTX's 8 bit signed value
    if (openTxRSSI > 127)
        openTxRSSI = 127;
    // convert to 8 bit signed value in the negative range (-128 to 0)
    openTxRSSI = 255 - openTxRSSI;
    Radio.TXdataBuffer[2] = openTxRSSI;

    Radio.TXdataBuffer[3] = (crsf.TLMbattSensor.voltage & 0xFF00) >> 8;
    Radio.TXdataBuffer[4] = crsf.LinkStatistics.uplink_SNR;
    Radio.TXdataBuffer[5] = crsf.LinkStatistics.uplink_Link_quality;
    Radio.TXdataBuffer[6] = (crsf.TLMbattSensor.voltage & 0x00FF);

    uint8_t crc = CalcCRC(Radio.TXdataBuffer, 7) + CRCCaesarCipher;
    Radio.TXdataBuffer[7] = crc;
    Radio.TXnb(Radio.TXdataBuffer, 8);
    return;
}

void ICACHE_RAM_ATTR HandleFreqCorr(bool value)
{
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
    NonceRX++;
    alreadyFHSS = false;
    linkQuality = getRFlinkQuality();
    incrementLQArray();
}

void ICACHE_RAM_ATTR HWtimerCallbackTock()
{
    HandleFHSS();
    HandleSendTelemetryResponse();
}

void ICACHE_RAM_ATTR LostConnection()
{
    if (connectionState == disconnected)
    {
        return; // Already disconnected
    }

    connectionStatePrev = connectionState;
    connectionState = disconnected; //set lost connection
    RXtimerState = tim_disconnected;
    FreqCorrection = 0;
    HWtimerError = 0;
    Offset = 0;
    prevOffset = 0;
    LPF_Offset.init(0);

    digitalWrite(GPIO_PIN_LED, 0);        // turn off led
    Radio.SetFrequency(GetInitialFreq()); // in conn lost state we always want to listen on freq index 0
    hwTimer.stop();
    Serial.println("lost conn");

#ifdef PLATFORM_STM32
    digitalWrite(GPIO_PIN_LED_GREEN, LOW);
#endif
}

void ICACHE_RAM_ATTR TentativeConnection()
{
    hwTimer.init();
    connectionStatePrev = connectionState;
    connectionState = tentative;
    RXtimerState = tim_disconnected;
    Serial.println("tentative conn");
    FreqCorrection = 0;
    HWtimerError = 0;
    Offset = 0;
    prevOffset = 0;
    LPF_Offset.init(0);
    
}

void ICACHE_RAM_ATTR GotConnection()
{
    if (connectionState == connected)
    {
        return; // Already connected
    }

    connectionStatePrev = connectionState;
    connectionState = connected; //we got a packet, therefore no lost connection
    RXtimerState = tim_tentative;
    GotConnectionMillis = millis();

    RFmodeLastCycled = millis();   // give another 3 sec for loc to occur.
    digitalWrite(GPIO_PIN_LED, 1); // turn on led
    Serial.println("got conn");

#ifdef PLATFORM_STM32
    digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
#endif
}

void ICACHE_RAM_ATTR UnpackChannelData_11bit()
{
    crsf.PackedRCdataOut.ch0 = (Radio.RXdataBuffer[1] << 3) + ((Radio.RXdataBuffer[5] & 0b11100000) >> 5);
    crsf.PackedRCdataOut.ch1 = (Radio.RXdataBuffer[2] << 3) + ((Radio.RXdataBuffer[5] & 0b00011100) >> 2);
    crsf.PackedRCdataOut.ch2 = (Radio.RXdataBuffer[3] << 3) + ((Radio.RXdataBuffer[5] & 0b00000011) << 1) + (Radio.RXdataBuffer[6] & 0b10000000 >> 7);
    crsf.PackedRCdataOut.ch3 = (Radio.RXdataBuffer[4] << 3) + ((Radio.RXdataBuffer[6] & 0b01110000) >> 4);
#ifdef One_Bit_Switches
    crsf.PackedRCdataOut.ch4 = BIT_to_CRSF(Radio.RXdataBuffer[6] & 0b00001000);
    crsf.PackedRCdataOut.ch5 = BIT_to_CRSF(Radio.RXdataBuffer[6] & 0b00000100);
    crsf.PackedRCdataOut.ch6 = BIT_to_CRSF(Radio.RXdataBuffer[6] & 0b00000010);
    crsf.PackedRCdataOut.ch7 = BIT_to_CRSF(Radio.RXdataBuffer[6] & 0b00000001);
#endif
}

void ICACHE_RAM_ATTR UnpackChannelData_10bit()
{
    crsf.PackedRCdataOut.ch0 = UINT10_to_CRSF((Radio.RXdataBuffer[1] << 2) + ((Radio.RXdataBuffer[5] & 0b11000000) >> 6));
    crsf.PackedRCdataOut.ch1 = UINT10_to_CRSF((Radio.RXdataBuffer[2] << 2) + ((Radio.RXdataBuffer[5] & 0b00110000) >> 4));
    crsf.PackedRCdataOut.ch2 = UINT10_to_CRSF((Radio.RXdataBuffer[3] << 2) + ((Radio.RXdataBuffer[5] & 0b00001100) >> 2));
    crsf.PackedRCdataOut.ch3 = UINT10_to_CRSF((Radio.RXdataBuffer[4] << 2) + ((Radio.RXdataBuffer[5] & 0b00000011) >> 0));
}

void ICACHE_RAM_ATTR UnpackMSPData()
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = Radio.RXdataBuffer[1];
    packet.addByte(Radio.RXdataBuffer[3]);
    packet.addByte(Radio.RXdataBuffer[4]);
    packet.addByte(Radio.RXdataBuffer[5]);
    packet.addByte(Radio.RXdataBuffer[6]);
    crsf.sendMSPFrameToFC(&packet);
}

void ICACHE_RAM_ATTR ProcessRFPacket()
{
    noInterrupts();
    beginProcessing = micros();
    uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
    uint8_t inCRC = Radio.RXdataBuffer[7];
    uint8_t type = Radio.RXdataBuffer[0] & 0b11;
    uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;

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

    LastValidPacketPrevMicros = LastValidPacketMicros;
    LastValidPacketMicros = beginProcessing;
    LastValidPacket = millis();

    getRFlinkInfo();

    switch (type)
    {
    case RC_DATA_PACKET: //Standard RC Data Packet
        #if defined SEQ_SWITCHES
        UnpackChannelDataSeqSwitches(&Radio, &crsf);
        #elif defined HYBRID_SWITCHES_8
        UnpackChannelDataHybridSwitches8(&Radio, &crsf);
        #else
        UnpackChannelData_11bit();
        #endif
        crsf.sendRCFrameToFC();
        break;

    case MSP_DATA_PACKET:
        UnpackMSPData();
        break;

    case TLM_PACKET: //telemetry packet from master

        // not implimented yet
        break;

    case SYNC_PACKET: //sync packet from master
        if (Radio.RXdataBuffer[4] == UID[3] && Radio.RXdataBuffer[5] == UID[4] && Radio.RXdataBuffer[6] == UID[5])
        {
            LastSyncPacket = millis();
            #ifndef DEBUG_SUPPRESS
            Serial.println("sync");
            #endif

            if (connectionState == disconnected)
            {
                TentativeConnection();
            }

            if (connectionState == tentative && NonceRX == Radio.RXdataBuffer[2] && FHSSgetCurrIndex() == Radio.RXdataBuffer[1] && OffsetDx <= 5 && linkQuality > 75)
                GotConnection();

            expresslrs_RFrates_e rateIn = (expresslrs_RFrates_e)((Radio.RXdataBuffer[3] & 0b11100000) >> 5);
            uint8_t TLMrateIn = ((Radio.RXdataBuffer[3] & 0b00011100) >> 2);

            if ((ExpressLRS_currAirRate_Modparams->enum_rate != rateIn) || (ExpressLRS_currAirRate_Modparams->TLMinterval != (expresslrs_tlm_ratio_e)TLMrateIn))
            { // change link parameters if required
                #ifndef DEBUG_SUPPRESS
                Serial.println("New TLMrate: ");
                Serial.println(TLMrateIn);
                #endif
                ExpressLRS_AirRateNeedsUpdate = true;
                ExpressLRS_currAirRate_Modparams = get_elrs_airRateConfig((expresslrs_RFrates_e)rateIn);
                ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)TLMrateIn;
            }

            FHSSsetCurrIndex(Radio.RXdataBuffer[1]);
            NonceRX = Radio.RXdataBuffer[2];
        }
        break;

    default: // code to be executed if n doesn't match any cases
        break;
    }

    HandleFHSS();
    HandleSendTelemetryResponse();
    addPacketToLQ(); // Adds packet to LQ otherwise an artificial drop in LQ is seen due to sending TLM.

    if (connectionState != disconnected)
    {
        HWtimerError = ((LastValidPacketMicros - hwTimer.LastCallbackMicrosTick) % ExpressLRS_currAirRate_Modparams->interval);
        RawOffset = HWtimerError - (ExpressLRS_currAirRate_Modparams->interval >> 1);
        OffsetDx = LPF_OffsetDx.update(abs(RawOffset - prevOffset));
        Offset = LPF_Offset.update(RawOffset); //crude 'locking function' to lock hardware timer to transmitter, seems to work well enough
        prevOffset = Offset;

        hwTimer.phaseShift((Offset >> 2) + timerOffset);
    }

    if ((alreadyFHSS == false) || (ExpressLRS_currAirRate_Modparams->enum_rate > 2)) 
    {
        HandleFreqCorr(Radio.GetFrequencyErrorbool()); //corrects for RX freq offset
        Radio.SetPPMoffsetReg(FreqCorrection);         //as above but corrects a different PPM offset based on freq error
    }

    doneProcessing = micros();
    Serial.print(RawOffset);
    Serial.print(":");
    Serial.print(Offset);
    Serial.print(":");
    Serial.print(OffsetDx);
    Serial.print(":");
    Serial.println(linkQuality);
    crsf.RXhandleUARTout();
    
    interrupts();
}

void beginWebsever()
{
#ifdef PLATFORM_STM32
#else
    hwTimer.stop();

    BeginWebUpdate();
    webUpdateMode = true;
#endif
}

void ICACHE_RAM_ATTR sampleButton()
{
    bool buttonValue = digitalRead(GPIO_PIN_BUTTON);

    if (buttonValue == false && buttonPrevValue == true)
    { //falling edge
        buttonLastPressed = millis();
        buttonDown = true;
        Serial.println("Manual Start");
        Radio.SetFrequency(GetInitialFreq());
        Radio.RXnb();
    }

    if (buttonValue == true && buttonPrevValue == false) //rising edge
    {
        buttonDown = false;
    }

    if ((millis() > buttonLastPressed + WEB_UPDATE_PRESS_INTERVAL) && buttonDown) // button held down
    {
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
}

void ICACHE_RAM_ATTR RXdoneISR()
{
    ProcessRFPacket();
    //Serial.println("r");
}

void ICACHE_RAM_ATTR TXdoneISR()
{
    alreadyTLMresp = false;
    addPacketToLQ(); // Adds packet to LQ otherwise an artificial drop in LQ is seen due to sending TLM.
    Radio.RXnb();
}

void setup()
{
    delay(100);
    Serial.println("ExpressLRS Module Booting...");

#ifdef PLATFORM_STM32
    Serial.setTx(GPIO_PIN_RCSIGNAL_TX);
    Serial.setRx(GPIO_PIN_RCSIGNAL_RX);
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    pinMode(GPIO_PIN_BUTTON, INPUT);
#endif

#ifdef PLATFORM_ESP8266
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    pinMode(GPIO_PIN_LED, OUTPUT);
#endif

    // Serial.begin(230400); // for linux debugging
    Serial.begin(420000);
    
    FHSSrandomiseFHSSsequence();

    Radio.currFreq = GetInitialFreq();
    Radio.Begin();
    Radio.SetSyncWord(UID[3]);
    Radio.SetOutputPower(0b1111); //default is max power (17dBm for RX always)

    // RFnoiseFloor = MeasureNoiseFloor(); TODO disabled for now
    // Serial.print("RF noise floor: ");
    // Serial.print(RFnoiseFloor);
    // Serial.println("dBm");

    Radio.RXdoneCallback = &RXdoneISR;
    Radio.TXdoneCallback = &TXdoneISR;

    
    hwTimer.callbackTock = &HWtimerCallbackTock;
    hwTimer.callbackTick = &HWtimerCallbackTick;

    SetRFLinkRate(RATE_200HZ);
    crsf.Begin();
}

void loop()
{

    //Serial.println(linkQuality);
    //
    //Serial.print(headroom);
    //Serial.print(" Head2:");
    //Serial.println(headroom2);
    //crsf.RXhandleUARTout(); using interrupt based printing at the moment

    if ((connectionState == tentative) && (linkQuality < 85) && (millis() > (LastSyncPacket + ExpressLRS_currAirRate_RFperfParams->RFmodeCycleAddtionalTime)))
    {
        LostConnection();
        Serial.println("Bad sync, aborting");
        Radio.SetFrequency(GetInitialFreq());
        SetRFLinkRate(ExpressLRS_currAirRate_Modparams->enum_rate); //switch between 200hz, 100hz, 50hz, rates
        scanIndex = RATE_MAX - ExpressLRS_currAirRate_Modparams->enum_rate;
        RFmodeLastCycled = millis();
        NonceRX = 0;
    }

    if (millis() > 60000)
    {
        CURR_RATE_MAX = RATE_MAX; //switch between 200hz, 100hz, 50hz, 25hz, 4hz rates
    }
    else
    {
        CURR_RATE_MAX = 3; //switch between 200hz, 100hz, 50hz, rates
    }

    if (millis() > (RFmodeLastCycled + ExpressLRS_currAirRate_RFperfParams->RFmodeCycleInterval)) // connection = tentative we add alittle delay
    {
        if ((connectionState == disconnected) && !webUpdateMode)
        {
            Radio.SetFrequency(GetInitialFreq());
            LastSyncPacket = millis();                            // reset this variable
            SetRFLinkRate((expresslrs_RFrates_e)(scanIndex % CURR_RATE_MAX)); //switch between rates
            LQreset();
            digitalWrite(GPIO_PIN_LED, LED);
            LED = !LED;
            Serial.println(ExpressLRS_currAirRate_Modparams->interval);
            scanIndex++;
        }
        RFmodeLastCycled = millis();
    }

    if ((millis() > (LastValidPacket + ExpressLRS_currAirRate_RFperfParams->RFmodeCycleInterval)) || ((millis() > (LastSyncPacket + 11000)) && linkQuality < 10)) // check if we lost conn.
    {
        LostConnection();
    }

    if ((millis() > (SendLinkStatstoFCintervalLastSent + SEND_LINK_STATS_TO_FC_INTERVAL)) && connectionState != disconnected)
    {
        crsf.sendLinkStatisticsToFC();
        SendLinkStatstoFCintervalLastSent = millis();
        #ifndef DEBUG_SUPPRESS
        //Serial.print(Offset);
        //Serial.print(":");
        //Serial.print(OffsetDx);
        //Serial.print(":");
        //Serial.println(linkQuality);
        #endif
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

    #ifdef Auto_WiFi_On_Boot
    if ((connectionState == disconnected) && !webUpdateMode && millis() > 20000 && millis() < 21000)
    {
        beginWebsever();
    }
    #endif

    #ifdef PLATFORM_STM32
    STM32_RX_HandleUARTin();
    #endif

    #ifdef PLATFORM_ESP8266
    if (webUpdateMode)
    {
        HandleWebUpdate();
        if (millis() > WEB_UPDATE_LED_FLASH_INTERVAL + webUpdateLedFlashIntervalLast)
        {
            digitalWrite(GPIO_PIN_LED, LED);
            LED = !LED;
            webUpdateLedFlashIntervalLast = millis();
        }
    }
    #endif
    yield();
}

#include <Arduino.h>
#include "targets.h"
#include "utils.h"
#include "common.h"
#include "LowPassFilter.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"
#include "FHSS.h"
// #include "Debug.h"
#include "rx_LinkQuality.h"
#include "errata.h"
#include "UpdateRFrateTLMrate.h"

#ifdef PLATFORM_ESP8266
#include "ESP8266_WebUpdate.h"
#include "ESP8266_hwTimer.h"
#endif

#ifdef PLATFORM_STM32
#include "STM32_UARTinHandler.h"
#include "STM32_hwTimer.h"
#endif

void ICACHE_RAM_ATTR SetRFLinkRate(expresslrs_mod_settings_s mode);
void ICACHE_RAM_ATTR TentativeConnection();

hwTimer hwTimer;

SX127xDriver Radio;
CRSF crsf(Serial); //pass a serial port object to the class for it to use

/// Filters ////////////////
LPF LPF_PacketInterval(3);
LPF LPF_Offset(3);
LPF LPF_FreqError(3);
LPF LPF_UplinkRSSI(5);
////////////////////////////

uint8_t scanIndex = 0;

int32_t HWtimerError;
int32_t Offset;

uint32_t SerialDebugPrintInterval = 250;
uint32_t LastSerialDebugPrint = 0;

bool LED = false;

//// Variables Relating to Button behaviour ////
bool buttonPrevValue = true; //default pullup
bool buttonDown = false;     //is the button current pressed down?
uint32_t buttonSampleInterval = 150;
uint32_t buttonLastSampled = 0;
uint32_t buttonLastPressed = 0;
uint32_t webUpdatePressInterval = 2000; //hold button for 2 sec to enable webupdate mode
uint32_t buttonResetInterval = 4000;    //hold button for 4 sec to reboot RX
bool webUpdateMode = false;

uint32_t webUpdateLedFlashInterval = 25;
uint32_t webUpdateLedFlashIntervalLast;
///////////////////////////////////////////////

volatile uint8_t NonceRXlocal = 0; // nonce that we THINK we are up to.

bool alreadyFHSS = false;
bool alreadyTLMresp = false;

//////////////////////////////////////////////////////////////

///////Variables for Telemetry and Link Quality///////////////
int packetCounter = 0;
int CRCerrorCounter = 0;
float CRCerrorRate = 0;
uint32_t PacketRateLastChecked = 0;
uint32_t PacketRateInterval = 1000;

float PacketRate = 0.0;

uint32_t LastValidPacketMicros = 0;
uint32_t LastValidPacketPrevMicros = 0; //Previous to the last valid packet (used to measure the packet interval)
uint32_t LastValidPacket = 0;           //Time the last valid packet was recv

uint32_t SendLinkStatstoFCinterval = 100;
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

    crsf.LinkStatistics.uplink_RSSI_1 = (LPF_UplinkRSSI.update(Radio.GetLastPacketRSSI()) + 130);
    crsf.LinkStatistics.uplink_RSSI_2 = 0;
    crsf.LinkStatistics.uplink_SNR = Radio.GetLastPacketSNR() * 10;
    crsf.LinkStatistics.uplink_Link_quality = linkQuality;
    crsf.LinkStatistics.rf_Mode = ExpressLRS_currAirRate.enum_rate;

    //Serial.println(crsf.LinkStatistics.uplink_RSSI_1);
}

void ICACHE_RAM_ATTR HandleAirRateUpdate()
{
    if (ExpressLRS_RateUpdateTime == true)
    {
        if (updateNonceCounter > 3)
        {
            ExpressLRS_RateUpdateTime = false;
            Serial.println("HandleAirRateUpdate");
            Serial.print("new: ");
            //Serial.println(ExpressLRS_AirRateConfig[1].interval);
            //SetRFLinkRate(ExpressLRS_AirRateConfig[(uint8_t)ExpressLRS_nextAirRate.enum_rate]);
            TentativeConnection();
            Serial.println(ExpressLRS_nextAirRate.interval);
            SetRFLinkRate(ExpressLRS_nextAirRate);
            updateNonceCounter = 0;
        }
        else
        {
            updateNonceCounter++;
        }
    }
}

void ICACHE_RAM_ATTR HandleFHSS()
{
    uint8_t modresult = (NonceRXlocal + 1) % ExpressLRS_currAirRate.FHSShopInterval;

    if (modresult == 0)
    {
        linkQuality = getRFlinkQuality();
        if (connectionState != disconnected) // don't hop if we lost
        {
            Radio.SetFrequency(FHSSgetNextFreq());
            Radio.RXnb();
            //crsf.sendLinkStatisticsToFC();
        }
    }
}

uint8_t ICACHE_RAM_ATTR FSMpacket()
{
    uint8_t Data1 = ((((uint8_t)FSMratesGetState()) & 0b00000011) << 6);
    uint8_t Data2 = ((((uint8_t)ExpressLRS_nextAirRate.TLMinterval) & 0b00000111) << 3);
    uint8_t Data3 = ((((uint8_t)ExpressLRS_nextAirRate.enum_rate) & 0b00000111) << 0);

    return Data1 + Data2 + Data3;
    //return Data1;
}

void ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    if (connectionState == connected || connectionState == tentative) // don't bother sending tlm if disconnected
    {
        uint8_t modresult = (NonceRXlocal + 1) % TLMratioEnumToValue(ExpressLRS_currAirRate.TLMinterval);

        if (modresult == 0)
        {
            Radio.TXdataBuffer[0] = (DeviceAddr << 2) + 0b11; // address + tlm packet
            Radio.TXdataBuffer[1] = CRSF_FRAMETYPE_LINK_STATISTICS;
            Radio.TXdataBuffer[2] = crsf.LinkStatistics.uplink_RSSI_1;
            //Radio.TXdataBuffer[3] = 0;
            Radio.TXdataBuffer[3] = FSMpacket(); // this will be the TLM rate and RF rate uplink channel.
            Radio.TXdataBuffer[4] = crsf.LinkStatistics.uplink_SNR;
            Radio.TXdataBuffer[5] = crsf.LinkStatistics.uplink_Link_quality;

            uint8_t crc = CalcCRC(Radio.TXdataBuffer, 7) + CRCCaesarCipher;
            Radio.TXdataBuffer[7] = crc;
            Radio.TXnb(Radio.TXdataBuffer, 8);
            addPacketToLQ(); // Adds packet to LQ otherwise an artificial drop in LQ is seen due to sending TLM.
        }
    }
}

//expresslrs packet header types
// 00 -> standard 4 channel data packet
// 01 -> switch data packet
// 11 -> tlm packet
// 10 -> sync packet with hop data

void ICACHE_RAM_ATTR HWtimerCallback()
{

    if (alreadyFHSS == true)
    {

        alreadyFHSS = false;
    }
    else
    {
        HandleFHSS();
    }

    incrementLQArray();
    HandleSendTelemetryResponse();

    NonceRXlocal++;
}

void ICACHE_RAM_ATTR LostConnection()
{
    if (connectionState != disconnected)
    {
        connectionStatePrev = connectionState;
        connectionState = disconnected; //set lost connection
        LPF_FreqError.init(0);

        digitalWrite(GPIO_PIN_LED, 0);        // turn off led
        Radio.SetFrequency(GetInitialFreq()); // in conn lost state we always want to listen on freq index 0
        Serial.println("lost conn");

#ifdef PLATFORM_STM32
        digitalWrite(GPIO_PIN_LED_GEEN, LOW);
#endif
    }
}

void ICACHE_RAM_ATTR TentativeConnection()
{
    connectionStatePrev = connectionState;
    connectionState = tentative;
    Serial.println("tentative conn");
    digitalWrite(GPIO_PIN_LED, 0); // turn on led
    RFmodeLastCycled = millis();   // give another 3 sec for loc to occur.
}

void ICACHE_RAM_ATTR GotConnection()
{
    if (connectionState != connected)
    {
        connectionStatePrev = connectionState;
        connectionState = connected; //we got a packet, therefore no lost connection

        digitalWrite(GPIO_PIN_LED, 1); // turn on led
        Serial.println("got conn");

#ifdef PLATFORM_STM32
        digitalWrite(GPIO_PIN_LED_GEEN, HIGH);
#endif
    }
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

void ICACHE_RAM_ATTR UnpackSwitchData()
{
    crsf.PackedRCdataOut.ch4 = SWITCH3b_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b11100000) >> 5); //unpack the byte structure, each switch is stored as a possible 8 states (3 bits). we shift by 2 to translate it into the 0....1024 range like the other channel data.
    crsf.PackedRCdataOut.ch5 = SWITCH3b_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b00011100) >> 2);
    crsf.PackedRCdataOut.ch6 = SWITCH3b_to_CRSF((uint16_t)((Radio.RXdataBuffer[1] & 0b00000011) << 1) + ((Radio.RXdataBuffer[2] & 0b10000000) >> 7));
    crsf.PackedRCdataOut.ch7 = SWITCH3b_to_CRSF((uint16_t)((Radio.RXdataBuffer[2] & 0b01110000) >> 4));
}

void ICACHE_RAM_ATTR ProcessRFPacket()
{
    uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
    uint8_t inCRC = Radio.RXdataBuffer[7];
    uint8_t type = Radio.RXdataBuffer[0] & 0b11;
    uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;

    if (inCRC == calculatedCRC)
    {
        if (packetAddr == DeviceAddr)
        {
            LastValidPacketPrevMicros = LastValidPacketMicros;
            LastValidPacketMicros = micros();
            LastValidPacket = millis();
            packetCounter++;

#ifdef TARGET_R9M_RX
            digitalWrite(PA8, HIGH);
#endif

            getRFlinkInfo();

            switch (type)
            {

            case 0b00: //Standard RC Data Packet
                UnpackChannelData_11bit();
                //crsf.sendRCFrameToFC();
                break;

            case 0b01:                                                                                                    // Switch Data Packet
                if ((Radio.RXdataBuffer[3] == Radio.RXdataBuffer[1]) && (Radio.RXdataBuffer[4] == Radio.RXdataBuffer[2])) // extra layer of protection incase the crc and addr headers fail us.
                {
                    UnpackSwitchData();
                    NonceRXlocal = Radio.RXdataBuffer[5];
                    FHSSsetCurrIndex(Radio.RXdataBuffer[6]);
                    //crsf.sendRCFrameToFC();
                }
                break;

            case 0b11: //telemetry packet from master

                // not implimented yet
                break;

            case 0b10: //sync packet from master
                if (Radio.RXdataBuffer[4] == TxBaseMac[3] && Radio.RXdataBuffer[5] == TxBaseMac[4] && Radio.RXdataBuffer[6] == TxBaseMac[5])
                {
                    if (connectionState == disconnected)
                    {
                        TentativeConnection();
                    }

                    if (connectionState == tentative && NonceRXlocal == Radio.RXdataBuffer[2] && FHSSgetCurrIndex() == Radio.RXdataBuffer[1] && linkQuality > 95)
                    {
                        GotConnection();
                    }

                    //ExpressLRS_nextAirRate.enum_rate = (expresslrs_RFrates_e)(Radio.RXdataBuffer[3] & 0b00000111);
                    //ExpressLRS_nextAirRate.TLMinterval = (expresslrs_tlm_ratio_e)((Radio.RXdataBuffer[3] & 0b00111000) >> 3);
                    ExpressLRS_nextAirRate = ExpressLRS_AirRateConfig[(Radio.RXdataBuffer[3] & 0b00000111)];
                    FSMratesNewEventRX((rates_updater_fsm_)((Radio.RXdataBuffer[3] & 0b11000000) >> 6));
                    //FSMUpdateState((rates_updater_fsm_)((Radio.RXdataBuffer[3] & 0b11000000) >> 6));

                    // if (ExpressLRS_currAirRate.enum_rate == !(expresslrs_RFrates_e)(Radio.RXdataBuffer[2] & 0b00001111))
                    // {
                    //     Serial.println("update air rate");
                    //     SetRFLinkRate(ExpressLRS_AirRateConfig[Radio.RXdataBuffer[3]]);
                    //     ExpressLRS_currAirRate = ExpressLRS_AirRateConfig[Radio.RXdataBuffer[3]];
                    // }

                    FHSSsetCurrIndex(Radio.RXdataBuffer[1]);
                    NonceRXlocal = Radio.RXdataBuffer[2];
                }
                break;

            default: // code to be executed if n doesn't match any cases
                break;
            }

            addPacketToLQ();

            HWtimerError = ((micros() - hwTimer.LastCallbackMicrosTick) % ExpressLRS_currAirRate.interval);
            Offset = LPF_Offset.update(HWtimerError - (ExpressLRS_currAirRate.interval >> 1)); //crude 'locking function' to lock hardware timer to transmitter, seems to work well enough
            //Offset = HWtimerError - (ExpressLRS_currAirRate.interval >> 1); //crude 'locking function' to lock hardware timer to transmitter, seems to work well enough
            //hwTimer.phaseShift((int32_t(Offset >> 3)) + timerOffset);
            if (ExpressLRS_currAirRate.enum_rate == RATE_50HZ)
            {
                hwTimer.phaseShift(int32_t(Offset >> 1) + timerOffset);
            }
            else
            {
                //hwTimer.phaseShift(((int32_t(Offset >> (3 - (uint8_t)ExpressLRS_currAirRate.enum_rate)))) + timerOffset);
                hwTimer.phaseShift(((int32_t(Offset >> (4 - (uint8_t)ExpressLRS_currAirRate.enum_rate)))) + timerOffset);
            }

            if (((NonceRXlocal + 1) % ExpressLRS_currAirRate.FHSShopInterval) == 0) //premept the FHSS if we already know we'll have to do it next timer tick.
            {
                int32_t freqerror = LPF_FreqError.update(Radio.GetFrequencyError());
                Serial.print(FreqCorrection);
                Serial.print(" : ");
                Serial.println(Offset);

                if (freqerror > 0)
                {
                    if (FreqCorrection < FreqCorrectionMax)
                    {
                        FreqCorrection += 61; //min freq step is ~ 61hz
                    }
                    else
                    {
                        FreqCorrection = FreqCorrectionMax;
                        Serial.println("Max pos reasontable freq offset correction limit reached!");
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
                        Serial.println("Max neg reasontable freq offset correction limit reached!");
                    }
                }

                Radio.setPPMoffsetReg(FreqCorrection);

                //Serial.println(FreqCorrection);

                HandleFHSS();
                alreadyFHSS = true;
            }
            //Serial.println("");
            //Serial.print("Offset: ");
            //Serial.println(Offset);
            // Serial.print(" LQ: ");
            // Serial.println(linkQuality);
            //Serial.print(":");
            //Serial.println(PacketInterval);
#ifdef TARGET_R9M_RX
            digitalWrite(PA8, LOW);
#endif
        }
        else
        {
            Serial.println("wrong address");
        }
    }
    else
    {
        CRCerrorCounter++;
    }
}

void beginWebsever()
{
#ifdef PLATFORM_STM32
#else
    Radio.StopContRX();
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

    if ((millis() > buttonLastPressed + webUpdatePressInterval) && buttonDown) // button held down
    {
        if (!webUpdateMode)
        {
            beginWebsever();
        }
    }

    if ((millis() > buttonLastPressed + buttonResetInterval) && buttonDown)
    {
#ifdef PLATFORM_ESP8266
        ESP.restart();
#endif
    }

    buttonPrevValue = buttonValue;
}

void ICACHE_RAM_ATTR SetRFLinkRate(expresslrs_mod_settings_s mode) // Set speed of RF link (hz)
{
    Radio.StopContRX();
    Radio.Config(mode.bw, mode.sf, mode.cr, Radio.currFreq, Radio._syncWord);
    ExpressLRS_currAirRate = mode;
    hwTimer.updateInterval(mode.interval);
    //hwTimer.TimerDiv = (uint8_t)mode.enum_rate + 1;
    //LPF_PacketInterval.init(mode.interval);
    LPF_Offset.init(0);
    //InitHarwareTimer();
    Radio.RXnb();
}

void setup()
{
#ifdef PLATFORM_STM32
    Serial.setTx(GPIO_PIN_RCSIGNAL_TX);
    Serial.setRx(GPIO_PIN_RCSIGNAL_RX);
    Serial.begin(420000);
    crsf.InitSerial();
#endif

#ifdef TARGET_R9M_RX
    pinMode(PA8, OUTPUT);
#endif

#ifdef PLATFORM_ESP8266
    Serial.begin(420000);
#endif
    Serial.println("Module Booting...");
    pinMode(GPIO_PIN_LED, OUTPUT);

#ifdef PLATFORM_STM32
    pinMode(GPIO_PIN_LED_GEEN, OUTPUT);
#endif
    pinMode(GPIO_PIN_BUTTON, INPUT);

#ifdef Regulatory_Domain_AU_915
    Serial.println("Setting 915MHz Mode");
    Radio.RFmodule = RFMOD_SX1276; //define radio module here
#elif defined Regulatory_Domain_AU_433
    Serial.println("Setting 433MHz Mode");
    Radio.RFmodule = RFMOD_SX1278; //define radio module here
#endif

    FHSSrandomiseFHSSsequence();
    Radio.SetFrequency(GetInitialFreq());

    //Radio.SetSyncWord(0x122);

    Radio.Begin();

    Radio.SetOutputPower(0b1111); //default is max power (17dBm for RX)

    RFnoiseFloor = MeasureNoiseFloor();
    Serial.print("RF noise floor: ");
    Serial.print(RFnoiseFloor);
    Serial.println("dBm");

    Radio.RXdoneCallback1 = &ProcessRFPacket;

    Radio.TXdoneCallback1 = &HandleAirRateUpdate;
    Radio.TXdoneCallback2 = &Radio.RXnb;

    crsf.Begin();
    hwTimer.callbackTock = &HWtimerCallback;
    hwTimer.init();

    SetRFLinkRate(RF_RATE_200HZ);
    Radio.RXnb();
}

void loop()
{
    if (millis() > (RFmodeLastCycled + ExpressLRS_currAirRate.RFmodeCycleInterval + ((connectionState == tentative) ? ExpressLRS_currAirRate.RFmodeCycleAddtionalTime : 0))) // connection = tentative we add alittle delay
    {
        if ((connectionState == disconnected) && !webUpdateMode)
        {
            Radio.SetFrequency(GetInitialFreq());
            SetRFLinkRate(ExpressLRS_AirRateConfig[scanIndex % 3]); //switch between 200hz, 100hz, 50hz, rates
            //Radio.RXnb();
            LQreset();
            digitalWrite(GPIO_PIN_LED, LED);
            LED = !LED;
            Serial.println(ExpressLRS_currAirRate.interval);
            scanIndex++;
        }
        RFmodeLastCycled = millis();
    }

    if (millis() > (LastValidPacket + ExpressLRS_currAirRate.RFmodeCycleAddtionalTime)) // check if we lost conn.
    {
        LostConnection();
    }

    if ((millis() > (SendLinkStatstoFCintervalLastSent + SendLinkStatstoFCinterval)) && connectionState != disconnected)
    {
        //crsf.sendLinkStatisticsToFC();
        SendLinkStatstoFCintervalLastSent = millis();
    }

    if (millis() > (buttonLastSampled + buttonSampleInterval))
    {
        sampleButton();
        buttonLastSampled = millis();
    }

#ifdef Auto_WiFi_On_Boot
    if ((connectionState == disconnected) && !webUpdateMode && millis() > 10000 && millis() < 11000)
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
        if (millis() > webUpdateLedFlashInterval + webUpdateLedFlashIntervalLast)
        {
            digitalWrite(GPIO_PIN_LED, LED);
            LED = !LED;
            webUpdateLedFlashIntervalLast = millis();
        }
    }
#endif
}
#include <Arduino.h>
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"
#include "ESP8266_WebUpdate.h"
#include "FHSS.h"

SX127xDriver Radio;
CRSF crsf(Serial); //pass a serial port object to the class.

#define Regulatory_Domain_AU_433

///forward defs///
void SetRFLinkRate(expresslrs_mod_settings_s mode);
void InitOStimer();
void OStimerSetCallback(void (*CallbackFunc)(void));
void OStimerReset();
void OStimerUpdateInterval(uint32_t Interval);

void InitHarwareTimer();
void StopHWtimer();
void HWtimerSetCallback(void (*CallbackFunc)(void));
void HWtimerSetCallback90(void (*CallbackFunc)(void));
void HWtimerUpdateInterval(uint32_t Interval);
uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros();
uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros90();
void ICACHE_RAM_ATTR HWtimerPhaseShift(int16_t Offset);
uint32_t ICACHE_RAM_ATTR HWtimerGetIntervalMicros();

uint8_t scanIndex = 1;

uint8_t prevAirRate = 0;
uint8_t currAirRate = 0;

//uint32_t HWtimerLastcallback;
uint32_t MeasuredHWtimerInterval;
int32_t HWtimerError;
int32_t HWtimerError90;
int16_t Offset;
int16_t Offset90;

uint8_t testdata[7] = {1, 2, 3, 4, 5, 6, 7};

bool LED = false;

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

uint8_t DeviceAddr = 0b101010;

volatile uint8_t NonceRXlocal = 0; // nonce that we THINK we are up to.

//////////////////////////////////////////////////////////////

///////Variables for Telemetry and Link Quality///////////////
int packetCounter = 0;
uint32_t PacketRateLastChecked = 0;
uint32_t PacketRateInterval = 500;

float PacketRate = 0.0;
uint8_t linkQuality = 0;

uint32_t LostConnectionDelay = 1500; //after 1500ms we consider that we lost connection to the TX
bool LostConnection = true;
bool gotFHSSsync = false;
uint32_t LastValidPacket = 0; //Time the last valid packet was recv
///////////////////////////////////////////////////////////////

int offset = 0;

void ICACHE_RAM_ATTR HandleFHSS()
{
    //uint32_t startTime = micros();
    uint8_t modresult = (NonceRXlocal - offset) % ExpressLRS_currAirRate.FHSShopInterval;

    if (modresult == 0)
    {
        if (LostConnection == false) // don't hop if we lost
        {
            digitalWrite(16, LED);
            LED = !LED;
            Radio.SetFrequency(FHSSgetNextFreq());
            Radio.RXnb();
        }
        //Serial.println(micros() - startTime);
    }
}

void ICACHE_RAM_ATTR HandleFHSS_TX()
{
    //if (ExpressLRS_currAirRate.enum_rate != RATE_250HZ)
    // {
    if (LostConnection == false) // don't hop if we lost
    {
        digitalWrite(16, LED);
        LED = !LED;
        Radio.SetFrequency(FHSSgetNextFreq());
        Radio.RXnb();
    }
    else
    {
        Radio.RXnb();
    }
    //}
}

void ICACHE_RAM_ATTR HandleFHSS_RX()
{
    uint8_t modresult = NonceRXlocal % ExpressLRS_currAirRate.FHSShopInterval;

    if (modresult == 0)
    {
        if (LostConnection == false) // don't hop if we lost
        {
            digitalWrite(16, LED);
            LED = !LED;
            //Serial.println("hop");
            Radio.SetFrequency(FHSSgetNextFreq());
            Radio.RXnb();
        }
    }
}

void ICACHE_RAM_ATTR getRFlinkInfo()
{
    int8_t LastRSSI = Radio.GetLastPacketRSSI();

    // crsf.PackedRCdataOut.ch8 = UINT11_to_CRSF(map(LastRSSI, -100, -50, 0, 1023));
    // crsf.PackedRCdataOut.ch9 = UINT11_to_CRSF(fmap(linkQuality, 0, targetFrameRate, 0, 1023));

    crsf.LinkStatistics.uplink_RSSI_1 = (-Radio.GetLastPacketRSSI());
    crsf.LinkStatistics.uplink_RSSI_2 = 0;
    crsf.LinkStatistics.uplink_SNR = Radio.GetLastPacketSNR() * 10;
    crsf.LinkStatistics.uplink_Link_quality = linkQuality;

    //crsf.sendLinkStatisticsToFC();
}

void ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    getRFlinkInfo();

    Radio.TXdataBuffer[0] = (DeviceAddr << 2) + 0b11; // address + tlm packet
    Radio.TXdataBuffer[1] = CRSF_FRAMETYPE_LINK_STATISTICS;
    Radio.TXdataBuffer[2] = 120 + crsf.LinkStatistics.uplink_RSSI_1;
    Radio.TXdataBuffer[3] = 0;
    Radio.TXdataBuffer[4] = crsf.LinkStatistics.uplink_SNR;
    Radio.TXdataBuffer[5] = crsf.LinkStatistics.uplink_Link_quality;

    uint8_t crc = CalcCRC(Radio.TXdataBuffer, 6);
    Radio.TXdataBuffer[6] = crc;
    //delayMicroseconds(5000);
    Radio.TXnb(Radio.TXdataBuffer, 7);
    //radio hops after transmission of telemetry Packet
    //Radio.TXdoneCallback = &Radio.StartContRX;
    //Serial.println(crsf.LinkStatistics.uplink_RSSI_1);
}

//expresslrs packet header types
// 00 -> standard 4 channel data packet
// 01 -> switch data packet
// 11 -> tlm packet
// 10 -> sync packet with hop data

void ICACHE_RAM_ATTR Test90()
{
    NonceRXlocal++;
    HandleFHSS();
}

void ICACHE_RAM_ATTR Test()
{

    MeasuredHWtimerInterval = micros() - HWtimerGetlastCallbackMicros();

    digitalWrite(16, LED);
    LED = !LED;

    // if (millis() > (PacketRateLastChecked + PacketRateInterval)) //just some debug data
    // {
    //     PacketRateLastChecked = millis();
    //     Serial.println(MeasuredOStimerInterval);
    //     Serial.println(HWtimerError);
    // }
}

///////////Super Simple Lowpass for 'PLL' (not really a PLL)/////////
int RawData;
int32_t SmoothDataINT;
int32_t SmoothDataFP;
int Beta = 3;     // Length = 16
int FP_Shift = 3; //Number of fractional bits

int16_t ICACHE_RAM_ATTR SimpleLowPass(int16_t Indata)
{
    RawData = Indata;
    RawData <<= FP_Shift; // Shift to fixed point
    SmoothDataFP = (SmoothDataFP << Beta) - SmoothDataFP;
    SmoothDataFP += RawData;
    SmoothDataFP >>= Beta;
    // Don't do the following shift if you want to do further
    // calculations in fixed-point using SmoothData
    SmoothDataINT = SmoothDataFP >> FP_Shift;
    return SmoothDataINT;
}
//////////////////////////////////////////////////////////////////////

void ICACHE_RAM_ATTR SendCRSFframe()
{
}

void ICACHE_RAM_ATTR ProcessRFPacket()
{
    //Serial.println(".");
    uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 6);
    uint8_t inCRC = Radio.RXdataBuffer[6];
    uint8_t type = Radio.RXdataBuffer[0] & 0b11;
    uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;

    if (packetAddr == DeviceAddr)
    {
        if ((inCRC == calculatedCRC))
        {
            packetCounter++;

            LastValidPacket = millis();

            if (LostConnection)
            {
                InitHarwareTimer();
                LostConnection = false; //we got a packet, therefore no lost connection
                Serial.println("got conn");
            }

            //digitalWrite(16, LED);
            //LED = !LED;

            HWtimerError = micros() - HWtimerGetlastCallbackMicros();

            HWtimerError90 = micros() - HWtimerGetlastCallbackMicros90();

            uint32_t HWtimerInterval = HWtimerGetIntervalMicros();
            Offset = SimpleLowPass(HWtimerError - (ExpressLRS_currAirRate.interval / 2) + 300); //crude 'locking function' to lock hardware timer to transmitter, seems to work well enough
            HWtimerPhaseShift(Offset / 2);

            int8_t LastRSSI = Radio.GetLastPacketRSSI();

            crsf.PackedRCdataOut.ch8 = UINT11_to_CRSF(map(LastRSSI, -100, -30, 0, 1023));

            if (type == 0b00) //std 4 channel switch data
            {
                crsf.PackedRCdataOut.ch0 = UINT11_to_CRSF((Radio.RXdataBuffer[1] << 2) + (Radio.RXdataBuffer[5] & 0b11000000 >> 6));
                crsf.PackedRCdataOut.ch1 = UINT11_to_CRSF((Radio.RXdataBuffer[2] << 2) + (Radio.RXdataBuffer[5] & 0b00110000 >> 4));
                crsf.PackedRCdataOut.ch2 = UINT11_to_CRSF((Radio.RXdataBuffer[3] << 2) + (Radio.RXdataBuffer[5] & 0b00001100 >> 2));
                crsf.PackedRCdataOut.ch3 = UINT11_to_CRSF((Radio.RXdataBuffer[4] << 2) + (Radio.RXdataBuffer[5] & 0b00000011 >> 0));
                //crsf.sendRCFrameToFC();
            }

            if (type == 0b01)
            {
                //return;
                if ((Radio.RXdataBuffer[3] == Radio.RXdataBuffer[1]) && (Radio.RXdataBuffer[4] == Radio.RXdataBuffer[2])) // extra layer of protection incase the crc and addr headers fail us.
                {
                    crsf.PackedRCdataOut.ch4 = SWITCH3b_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b11100000) >> 5); //unpack the byte structure, each switch is stored as a possible 8 states (3 bits). we shift by 2 to translate it into the 0....1024 range like the other channel data.
                    crsf.PackedRCdataOut.ch5 = SWITCH3b_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b00011100) >> 2);
                    crsf.PackedRCdataOut.ch6 = SWITCH3b_to_CRSF((uint16_t)((Radio.RXdataBuffer[1] & 0b00000011) << 1) + ((Radio.RXdataBuffer[2] & 0b10000000) >> 7));
                    crsf.PackedRCdataOut.ch7 = SWITCH3b_to_CRSF((uint16_t)((Radio.RXdataBuffer[2] & 0b01110000) >> 4));
                    //Serial.print(NonceRXlocal);
                    //Serial.print("-");
                    //Serial.println(Radio.RXdataBuffer[5]);
                    //NonceRXlocal = Radio.RXdataBuffer[5]; //reset nonce with master value
                    //InitHarwareTimer();
                    //OStimerReset();
                    //crsf.sendRCFrameToFC();
                }
            }

            if (type == 0b11)
            { //telemetry packet from master
            }

            if (type == 0b10)
            { //sync packet from master
                //Serial.println("Sync Packet");

                //Serial.print(NonceRXlocal);
                //Serial.print(" ");
                //Serial.println(Radio.RXdataBuffer[2]);

                FHSSsetCurrIndex(Radio.RXdataBuffer[1]);

                //if (NonceRXlocal == Radio.RXdataBuffer[2])
                //{
                //Serial.println("yay!");
                // }
                // else
                //{
                //   Serial.println(NonceRXlocal - Radio.RXdataBuffer[2]);

                NonceRXlocal = Radio.RXdataBuffer[2];
                // }

                //gotFHSSsync = true;

                //Radio.SetFrequency(FHSSgetCurrFreq());
                //Serial.println("freq");

                //char str[30];
                //sprintf(str, "%.2f", FHSSgetCurrFreq());
                //Serial.println(str);
                //Serial.println(Radio.RXdataBuffer[1]);

                if (ExpressLRS_currAirRate.enum_rate == !(expresslrs_RFrates_e)Radio.RXdataBuffer[3])
                {
                    Serial.println("update rate");
                    switch (Radio.RXdataBuffer[3])
                    {
                    case 0:
                        SetRFLinkRate(RF_RATE_200HZ);
                        ExpressLRS_currAirRate = RF_RATE_200HZ;
                        break;
                    case 1:
                        SetRFLinkRate(RF_RATE_125HZ);
                        ExpressLRS_currAirRate = RF_RATE_125HZ;
                        break;
                    case 2:
                        SetRFLinkRate(RF_RATE_50HZ);
                        ExpressLRS_currAirRate = RF_RATE_50HZ;
                        break;
                    default:
                        break;
                    }
                }

                //Serial.println()
            }
        }
        else
        {
            //Serial.println("crc failed");
        }
    }
    else
    {
        //Serial.println("wrong address");
    }
}

void beginWebsever()
{
    Radio.StopContRX();
    StopHWtimer();

    BeginWebUpdate();
    webUpdateMode = true;
}

void ICACHE_RAM_ATTR sampleButton()
{

    bool buttonValue = digitalRead(0);

    if (buttonValue == false && buttonPrevValue == true)
    { //falling edge
        buttonLastPressed = millis();
        buttonDown = true;
        Serial.println("Manual Start");
        Radio.SetFrequency(GetInitialFreq());
        Radio.StartContRX();
    }

    if (buttonValue == true && buttonPrevValue == false) //rising edge
    {
        buttonDown = false;
    }

    if ((millis() > buttonLastPressed + webUpdatePressInterval) && buttonDown)
    {
        if (!webUpdateMode)
        {
            beginWebsever();
        }
    }

    if ((millis() > buttonLastPressed + buttonResetInterval) && buttonDown)
    {
        ESP.restart();
    }

    buttonPrevValue = buttonValue;
}

void ICACHE_RAM_ATTR SetRFLinkRate(expresslrs_mod_settings_s mode) // Set speed of RF link (hz)
{
    Radio.StopContRX();
    Radio.Config(mode.bw, mode.sf, mode.cr, Radio.currFreq, Radio._syncWord);
    ExpressLRS_currAirRate = mode;
    HWtimerUpdateInterval(mode.interval);
    Radio.RXnb();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Module Booting...");
    pinMode(16, OUTPUT);
    pinMode(2, INPUT);
    //

    delay(200);
    digitalWrite(16, HIGH);
    delay(200);
    digitalWrite(16, LOW);
    delay(200);
    digitalWrite(16, HIGH);
    delay(200);
    digitalWrite(16, LOW);

#ifdef Regulatory_Domain_AU_915
    Serial.println("Setting 915MHz Mode");
    FHSSsetFreqMode(915);
    Radio.RFmodule = RFMOD_SX1276;        //define radio module here
    Radio.SetFrequency(GetInitialFreq()); //set frequency first or an error will occur!!!
#elif defined Regulatory_Domain_AU_433
    Serial.println("Setting 433MHz Mode");
    FHSSsetFreqMode(433);
    Radio.RFmodule = RFMOD_SX1278;        //define radio module here
    Radio.SetFrequency(GetInitialFreq()); //set frequency first or an error will occur!!!
#endif

    Radio.TXbuffLen = 7;
    Radio.RXbuffLen = 7;

    Radio.Begin();

    //crsf.InitSerial();
    Radio.SetOutputPower(0b0000);

    Radio.RXdoneCallback1 = &ProcessRFPacket;

    //Radio.RXdoneCallback2 = &HandleFHSS_RX;

    //Radio.TXdoneCallback1 = &HandleFHSS_TX;

    Radio.TXdoneCallback1 = &Radio.StartContRX;

    crsf.Begin();

    //Radio.StartContTX();

    HWtimerSetCallback(&Test);
    HWtimerSetCallback90(&Test90);
    //InitHarwareTimer();

    delay(250);
    SetRFLinkRate(RF_RATE_125HZ);
    Radio.SetPreambleLength(6);
    delay(250);

    //writeRegister(0x36, 0x02);
    //writeRegister(0x3a, 0x7F);
    //setRegValue(0x44, 1, 7, 7); // fast on hop
}

void loop()
{
    if (millis() > (LastValidPacket + LostConnectionDelay))
    {
        if (!LostConnection)
        {
            Serial.println("lost conn");
            LostConnection = true;
            gotFHSSsync = false;
            StopHWtimer();
            Radio.SetFrequency(GetInitialFreq());
            Radio.StartContRX();
            //Radio.StopContRX();
        }
    }

    // if (offset == 3)
    // {
    //     offset = -3;
    // }
    // else
    // {

    //     offset++;
    // }
    // Serial.println(offset);
    // delay(10000);

    if (LostConnection && !webUpdateMode)

    {
        switch (scanIndex)
        {
        case 1:
            SetRFLinkRate(RF_RATE_200HZ);
            break;
        case 2:
            SetRFLinkRate(RF_RATE_125HZ);
            break;
        case 3:
            SetRFLinkRate(RF_RATE_50HZ);
            break;

        default:
            break;
        }

        //Serial.println(Radio.currFreq);
        Radio.SetPreambleLength(6);

        digitalWrite(16, LED);
        LED = !LED;

        if (scanIndex == 3)
        {
            scanIndex = 1;
        }
        else
        {

            scanIndex++;
        }

        delay(1000);
    }

    // if (LostConnection && !webUpdateMode)
    // {
    //     //Serial.println("start preamble detect");
    //     uint8_t result = Radio.RunCAD(); // run cad to quickly detect the start of a packet
    //     //Serial.println("done preamble detect");

    //     if (result == PREAMBLE_DETECTED)
    //     {
    //         Serial.println("detect preamble");
    //         //LostConnection = false;
    //         uint8_t datain[7];
    //         Radio.RXsingle(datain, 7);

    //         uint8_t calculatedCRC = CalcCRC(datain, 6);
    //         uint8_t inCRC = datain[6];
    //         uint8_t type = datain[0] & 0b11;
    //         uint8_t packetAddr = (datain[0] & 0b11111100) >> 2;

    //         if ((inCRC == calculatedCRC))
    //         {
    //             if (type == 0b10) //sync packet
    //             {
    //                 Radio.StartContRX();
    //                 //LostConnection = false;
    //             }
    //         }

    //         // if ((inCRC == calculatedCRC))
    //         // {
    //         //     Serial.println("got crc");
    //         //     LastValidPacket = millis();
    //         //     if (type == 0b10) //sync packet
    //         //     {
    //         //         Radio.StartContRX();
    //         //         //FHSSsetCurrIndex(datain[1]);
    //         //         //NonceRXlocal = datain[2];
    //         //         Serial.println("got good sync from CAD");
    //         //         LostConnection = false;
    //         //         return;
    //         //     }
    //         // }
    //     }
    //     else
    //     {
    //         //Serial.println("f");
    //         delay(20);
    //         Radio.SetFrequency(FHSSgetNextFreq());
    //     }
    // }
    //else // we have connection, calculate the stats below.
    //{
    if (millis() > (PacketRateLastChecked + PacketRateInterval)) //just some debug data
    {
        float targetFrameRate = ExpressLRS_currAirRate.rate - ((ExpressLRS_currAirRate.rate) * (1.0 / ExpressLRS_currAirRate.TLMinterval));
        PacketRateLastChecked = millis();
        PacketRate = (float)packetCounter / (float)(PacketRateInterval);
        linkQuality = int(((float)PacketRate / (float)targetFrameRate) * 100000.0);

        if (linkQuality > 100)
        {
            linkQuality = 100;
        }

        packetCounter = 0;
        //Serial.println(linkQuality);
    }
    //}

    // Serial.print(MeasuredHWtimerInterval);
    // Serial.print(" ");
    // Serial.print(Offset);
    // Serial.print(" ");
    // Serial.print(HWtimerError);

    // Serial.print("----");

    // Serial.print(Offset90);
    // Serial.print(" ");
    // Serial.print(HWtimerError90);
    // Serial.print("----");
    // Serial.println(packetCounter);
    // delay(200);

    if (millis() > (buttonLastSampled + buttonSampleInterval))
    {
        sampleButton();
        buttonLastSampled = millis();
    }

    //yield();

    if (webUpdateMode)
    {
        HandleWebUpdate();
        if (millis() > webUpdateLedFlashInterval + webUpdateLedFlashIntervalLast)
        {
            digitalWrite(16, LED);
            LED = !LED;
            webUpdateLedFlashIntervalLast = millis();
        }
    }
}
#include <Arduino.h>
#include "FIFO.h"
#include "utils.h"
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"
#include "FHSS.h"
#include "targets.h"
#include "HwTimer.h"
#include "debug.h"

HwTimer hwTimer;

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT 1500U // After 1500ms of no TLM response consider that slave has lost connection
#define PACKET_RATE_INTERVAL 500
#define RF_MODE_CYCLE_INTERVAL 1000
#define SWITCH_PACKET_SEND_INTERVAL 200
#define SYNC_PACKET_SEND_INTERVAL_RX_LOST 250  // how often to send the switch data packet (ms) when there is no response from RX
#define SYNC_PACKET_SEND_INTERVAL_RX_CONN 1500 // how often to send the switch data packet (ms) when there we have a connection
///////////////////

/// define some libs to use ///
SX127xDriver Radio;
CRSF crsf(CrsfSerial);

void TimerExpired();

//// Switch Data Handling ///////
uint32_t SwitchPacketNextSend = 0; //time in ms when the next switch data packet will be send

////////////SYNC PACKET/////////
uint32_t SyncPacketNextSend = 0;

uint32_t LastTLMpacketRecvMillis = 0;
bool isRXconnected = false;
int packetCounteRX_TX = 0;
uint32_t PacketRateNextCheck = 0;
float PacketRate = 0.0;
uint8_t linkQuality = 0;

/// Variables for Sync Behaviour ////
uint32_t RFmodeLastCycled = 0;
///////////////////////////////////////

bool UpdateParamReq = false;

bool Channels5to8Changed = false;

bool ChangeAirRateRequested = false;
bool ChangeAirRateSentUpdate = false;

bool WaitRXresponse = false;

///// Not used in this version /////////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t TelemetryWaitBuffer[7] = {0};

uint32_t LinkSpeedIncreaseDelayFactor = 500; // wait for the connection to be 'good' for this long before increasing the speed.
uint32_t LinkSpeedDecreaseDelayFactor = 200; // this long wait this long for connection to be below threshold before dropping speed

uint32_t LinkSpeedDecreaseFirstMetCondition = 0;
uint32_t LinkSpeedIncreaseFirstMetCondition = 0;

uint8_t LinkSpeedReduceSNR = 20;   //if the SNR (times 10) is lower than this we drop the link speed one level
uint8_t LinkSpeedIncreaseSNR = 60; //if the SNR (times 10) is higher than this we increase the link speed
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ICACHE_RAM_ATTR IncreasePower();
void ICACHE_RAM_ATTR DecreasePower();

void ICACHE_RAM_ATTR ProcessTLMpacket()
{
    uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
    uint8_t inCRC = Radio.RXdataBuffer[7];
    uint8_t type = Radio.RXdataBuffer[0] & TLM_PACKET;
    uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;
    uint8_t TLMheader = Radio.RXdataBuffer[1];

    //DEBUG_PRINTLN("TLMpacket0");

    if (packetAddr != DeviceAddr)
    {
        DEBUG_PRINTLN("TLM device address error");
        return;
    }

    if ((inCRC != calculatedCRC))
    {
        DEBUG_PRINTLN("TLM crc error");
        return;
    }

    packetCounteRX_TX++;

    if (type != TLM_PACKET)
    {
        DEBUG_PRINTLN("TLM type error");
        DEBUG_PRINTLN(type);
    }

    isRXconnected = true;
    LastTLMpacketRecvMillis = millis();

    if (TLMheader == CRSF_FRAMETYPE_LINK_STATISTICS)
    {
        crsf.LinkStatistics.uplink_RSSI_1 = Radio.RXdataBuffer[2];
        crsf.LinkStatistics.uplink_RSSI_2 = 0;
        crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
        crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];

        crsf.LinkStatistics.downlink_SNR = int(Radio.LastPacketSNR * 10);
        crsf.LinkStatistics.downlink_RSSI = 120 + Radio.LastPacketRSSI;
        crsf.LinkStatistics.downlink_Link_quality = linkQuality;
        //crsf.LinkStatistics.downlink_Link_quality = Radio.currPWR;
        crsf.LinkStatistics.rf_Mode = 4 - ExpressLRS_currAirRate->enum_rate;

        crsf.TLMbattSensor.voltage = (Radio.RXdataBuffer[3] << 8) + Radio.RXdataBuffer[6];

        crsf.sendLinkStatisticsToTX();
    }
}

void ICACHE_RAM_ATTR CheckChannels5to8Change()
{ //check if channels 5 to 8 have new data (switch channels)
    for (int i = 4; i < 8; i++)
    {
        if (crsf.ChannelDataInPrev[i] != crsf.ChannelDataIn[i])
        {
            Channels5to8Changed = true;
        }
    }
}

void ICACHE_RAM_ATTR GenerateSyncPacketData()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = (DeviceAddr << 2) + SYNC_PACKET;
    Radio.TXdataBuffer[0] = PacketHeaderAddr;
    Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
    Radio.TXdataBuffer[2] = Radio.NonceTX;
    Radio.TXdataBuffer[3] = 0;
    Radio.TXdataBuffer[4] = UID[3];
    Radio.TXdataBuffer[5] = UID[4];
    Radio.TXdataBuffer[6] = UID[5];
}

void ICACHE_RAM_ATTR Generate4ChannelData_10bit()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = (DeviceAddr << 2) + RC_DATA_PACKET;
    Radio.TXdataBuffer[0] = PacketHeaderAddr;
    Radio.TXdataBuffer[1] = ((CRSF_to_UINT10(crsf.ChannelDataIn[0]) & 0b1111111100) >> 2);
    Radio.TXdataBuffer[2] = ((CRSF_to_UINT10(crsf.ChannelDataIn[1]) & 0b1111111100) >> 2);
    Radio.TXdataBuffer[3] = ((CRSF_to_UINT10(crsf.ChannelDataIn[2]) & 0b1111111100) >> 2);
    Radio.TXdataBuffer[4] = ((CRSF_to_UINT10(crsf.ChannelDataIn[3]) & 0b1111111100) >> 2);
    Radio.TXdataBuffer[5] = ((CRSF_to_UINT10(crsf.ChannelDataIn[0]) & 0b0000000011) << 6) +
                            ((CRSF_to_UINT10(crsf.ChannelDataIn[1]) & 0b0000000011) << 4) +
                            ((CRSF_to_UINT10(crsf.ChannelDataIn[2]) & 0b0000000011) << 2) +
                            ((CRSF_to_UINT10(crsf.ChannelDataIn[3]) & 0b0000000011) << 0);
}

void ICACHE_RAM_ATTR Generate4ChannelData_11bit()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = (DeviceAddr << 2) + RC_DATA_PACKET;
    Radio.TXdataBuffer[0] = PacketHeaderAddr;
    Radio.TXdataBuffer[1] = ((crsf.ChannelDataIn[0]) >> 3);
    Radio.TXdataBuffer[2] = ((crsf.ChannelDataIn[1]) >> 3);
    Radio.TXdataBuffer[3] = ((crsf.ChannelDataIn[2]) >> 3);
    Radio.TXdataBuffer[4] = ((crsf.ChannelDataIn[3]) >> 3);
    Radio.TXdataBuffer[5] = ((crsf.ChannelDataIn[0] & 0b00000111) << 5) +
                            ((crsf.ChannelDataIn[1] & 0b111) << 2) +
                            ((crsf.ChannelDataIn[2] & 0b110) >> 1);
    Radio.TXdataBuffer[6] = ((crsf.ChannelDataIn[2] & 0b001) << 7) +
                            ((crsf.ChannelDataIn[3] & 0b111) << 4); // 4 bits left over for something else?
#ifdef One_Bit_Switches
    Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[4]) << 3;
    Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[5]) << 2;
    Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[6]) << 1;
    Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[7]) << 0;
#endif
}

void ICACHE_RAM_ATTR GenerateSwitchChannelData()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = (DeviceAddr << 2) + SWITCH_DATA_PACKET;
    Radio.TXdataBuffer[0] = PacketHeaderAddr;
    Radio.TXdataBuffer[1] = ((CRSF_to_UINT10(crsf.ChannelDataIn[4]) & 0b1110000000) >> 2) +
                            ((CRSF_to_UINT10(crsf.ChannelDataIn[5]) & 0b1110000000) >> 5) +
                            ((CRSF_to_UINT10(crsf.ChannelDataIn[6]) & 0b1100000000) >> 8);
    Radio.TXdataBuffer[2] = (CRSF_to_UINT10(crsf.ChannelDataIn[6]) & 0b0010000000) +
                            ((CRSF_to_UINT10(crsf.ChannelDataIn[7]) & 0b1110000000) >> 3);
    Radio.TXdataBuffer[3] = Radio.TXdataBuffer[1];
    Radio.TXdataBuffer[4] = Radio.TXdataBuffer[2];
    Radio.TXdataBuffer[5] = Radio.NonceTX;
    Radio.TXdataBuffer[6] = FHSSgetCurrIndex();
}

void ICACHE_RAM_ATTR SetRFLinkRate(expresslrs_RFrates_e rate) // Set speed of RF link (hz)
{
    const expresslrs_mod_settings_s *const mode = get_elrs_airRateConfig(rate);

//#ifdef PLATFORM_ESP32
#if 0
    Radio.TimerInterval = mode->interval;
    Radio.UpdateTimerInterval();
#else
    hwTimer.updateInterval(mode->interval); // TODO: Make sure this is equiv to above commented lines
#endif

    Radio.Config(mode->bw, mode->sf, mode->cr, Radio.currFreq, Radio._syncWord);
    Radio.SetPreambleLength(mode->PreambleLen);

    ExpressLRS_prevAirRate = ExpressLRS_currAirRate;
    ExpressLRS_currAirRate = mode;

    crsf.RequestedRCpacketInterval = mode->interval;

    isRXconnected = false;
#ifdef TARGET_R9M_TX
    //r9dac.resume();
#endif
}

uint8_t ICACHE_RAM_ATTR decRFLinkRate()
{
    DEBUG_PRINTLN("dec");
    if ((uint8_t)ExpressLRS_currAirRate->enum_rate < MaxRFrate)
    {
        SetRFLinkRate((expresslrs_RFrates_e)(ExpressLRS_currAirRate->enum_rate + 1));
    }
    return (uint8_t)ExpressLRS_currAirRate->enum_rate;
}

uint8_t ICACHE_RAM_ATTR incRFLinkRate()
{
    DEBUG_PRINTLN("inc");
    if ((uint8_t)ExpressLRS_currAirRate->enum_rate > RATE_200HZ)
    {
        SetRFLinkRate((expresslrs_RFrates_e)(ExpressLRS_currAirRate->enum_rate - 1));
    }
    return (uint8_t)ExpressLRS_currAirRate->enum_rate;
}

void ICACHE_RAM_ATTR HandleFHSS()
{
    uint8_t modresult = (Radio.NonceTX) % ExpressLRS_currAirRate->FHSShopInterval;

    if (modresult == 0) // if it time to hop, do so.
    {
        Radio.SetFrequency(FHSSgetNextFreq());
    }
}

void ICACHE_RAM_ATTR HandleTLM()
{
    if (ExpressLRS_currAirRate->TLMinterval > 0)
    {
        uint8_t modresult = (Radio.NonceTX) % TLMratioEnumToValue(ExpressLRS_currAirRate->TLMinterval);
        if (modresult != 0) // wait for tlm response because it's time
        {
            return;
        }

#ifdef TARGET_R9M_TX
        //r9dac.standby(); //takes too long
        digitalWrite(GPIO_PIN_RFswitch_CONTROL, 1);
        digitalWrite(GPIO_PIN_RFamp_APC1, 0);
#endif

        Radio.RXnb();
        WaitRXresponse = true;
    }
}

void ICACHE_RAM_ATTR SendRCdataToRF()
{
#ifdef FEATURE_OPENTX_SYNC
    crsf.UpdateOpenTxSyncOffset(); // tells the crsf that we want to send data now - this allows opentx packet syncing
#endif

    /////// This Part Handles the Telemetry Response ///////
    if (ExpressLRS_currAirRate->TLMinterval > 0)
    {
        uint8_t modresult = (Radio.NonceTX) % TLMratioEnumToValue(ExpressLRS_currAirRate->TLMinterval);
        if (modresult == 0)
        { // wait for tlm response
            if (WaitRXresponse == true)
            {
                WaitRXresponse = false;
                return;
            }
            else
            {
                Radio.NonceTX++;
            }
        }
    }

    uint32_t current_ms = millis();

    //if (((current_ms > SyncPacketNextSend) && (Radio.currFreq == GetInitialFreq())) || ChangeAirRateRequested) //only send sync when its time and only on channel 0;
    if ((current_ms > SyncPacketNextSend) && (Radio.currFreq == GetInitialFreq()))
    {
        uint32_t SyncInterval =
            (isRXconnected) ? SYNC_PACKET_SEND_INTERVAL_RX_CONN : SYNC_PACKET_SEND_INTERVAL_RX_LOST;

        GenerateSyncPacketData();
        SyncPacketNextSend = current_ms + SyncInterval;
        ChangeAirRateSentUpdate = true;
        //DEBUG_PRINTLN("sync");
        //DEBUG_PRINTLN(Radio.currFreq);
    }
    else
    {
        if ((current_ms > SwitchPacketNextSend) || Channels5to8Changed)
        {
            Channels5to8Changed = false;
            GenerateSwitchChannelData();
            SwitchPacketNextSend = current_ms + SWITCH_PACKET_SEND_INTERVAL;
        }
        else // else we just have regular channel data which we send as 8 + 2 bits
        {
            Generate4ChannelData_11bit();
        }
    }

    ///// Next, Calculate the CRC and put it into the buffer /////
    uint8_t crc = CalcCRC(Radio.TXdataBuffer, 7) + CRCCaesarCipher;
    Radio.TXdataBuffer[7] = crc;
#ifdef TARGET_R9M_TX
    //r9dac.resume(); takes too long
    digitalWrite(GPIO_PIN_RFswitch_CONTROL, 0);
    digitalWrite(GPIO_PIN_RFamp_APC1, 1);
#endif
    Radio.TXnb(Radio.TXdataBuffer, 8);

    if (ChangeAirRateRequested)
    {
        ChangeAirRateSentUpdate = true;
    }
}

void ICACHE_RAM_ATTR ParamUpdateReq()
{
    UpdateParamReq = true;
}

void ICACHE_RAM_ATTR HandleUpdateParameter()
{
    if (!UpdateParamReq)
    {
        return;
    }

    switch (crsf.ParameterUpdateData[0])
    {
    case 0: // send all params
        DEBUG_PRINTLN("send all");
        crsf.sendLUAresponse((ExpressLRS_currAirRate->enum_rate + 2), ExpressLRS_currAirRate->TLMinterval + 1, 7, 1);
        break;
    case 1:
        // if (ExpressLRS_currAirRate->enum_rate != (expresslrs_RFrates_e)crsf.ParameterUpdateData[1])
        // {
        //   SetRFLinkRate(ExpressLRS_AirRateConfig[crsf.ParameterUpdateData[1]]);
        // }
        //crsf.sendLUAresponse(0x01, (uint8_t)random(1, 5));
        if (crsf.ParameterUpdateData[1] == 0)
        {
            /*uint8_t newRate =*/decRFLinkRate();
        }
        else if (crsf.ParameterUpdateData[1] == 1)
        {
            /*uint8_t newRate =*/incRFLinkRate();
        }
        DEBUG_PRINTLN(ExpressLRS_currAirRate->enum_rate);
        crsf.sendLUAresponse((ExpressLRS_currAirRate->enum_rate + 2), ExpressLRS_currAirRate->TLMinterval + 1, 7, 1);
        break;

    case 2:

        break;
    case 3:

        switch (crsf.ParameterUpdateData[1])
        {
        case 0:
            Radio.maxPWR = 0b1111;
            Radio.SetOutputPower(0b1111); // 500 mW
            DEBUG_PRINTLN("Setpower 500 mW");
            break;

        case 1:
            Radio.maxPWR = 0b1000;
            Radio.SetOutputPower(0b1000);
            DEBUG_PRINTLN("Setpower 200 mW");
            break;

        case 2:
            Radio.maxPWR = 0b1000;
            Radio.SetOutputPower(0b1000);
            DEBUG_PRINTLN("Setpower 100 mW");
            break;

        case 3:
            Radio.maxPWR = 0b0101;
            Radio.SetOutputPower(0b0101);
            DEBUG_PRINTLN("Setpower 50 mW");
            break;

        case 4:
            Radio.maxPWR = 0b0010;
            Radio.SetOutputPower(0b0010);
            DEBUG_PRINTLN("Setpower 25 mW");
            break;

        case 5:
            Radio.maxPWR = 0b0000;
            Radio.SetOutputPower(0b0000);
            DEBUG_PRINTLN("Setpower Pit");
            break;

        default:
            break;
        }

        break;
    case 4:

        break;

    default:
        break;
    }

    UpdateParamReq = false;
}

void DetectOtherRadios()
{
    Radio.SetFrequency(GetInitialFreq());
    //Radio.RXsingle();

    // if (Radio.RXsingle(RXdata, 7, 2 * (RF_RATE_50HZ.interval / 1000)) == ERR_NONE)
    // {
    //   DEBUG_PRINTLN("got fastsync resp 1");
    //   break;
    // }
}

void setup()
{
    CrsfSerial.Begin(CRSF_OPENTX_BAUDRATE);

    platform_setup();

    crsf.connected = &hwTimer.init; // it will auto init when it detects UART connection
    crsf.disconnected = &hwTimer.stop;
    crsf.RecvParameterUpdate = &ParamUpdateReq;
    hwTimer.callbackTock = &TimerExpired;

    DEBUG_PRINTLN("ExpressLRS TX Module Booted...");

    FHSSrandomiseFHSSsequence();

#if defined Regulatory_Domain_AU_915 || defined Regulatory_Domain_EU_868 || defined Regulatory_Domain_FCC_915
#ifdef Regulatory_Domain_EU_868
    DEBUG_PRINTLN("Setting 868MHz Mode");
#else
    DEBUG_PRINTLN("Setting 915MHz Mode");
#endif

    Radio.RFmodule = RFMOD_SX1276; //define radio module here
#ifdef TARGET_100mW_MODULE
    Radio.SetOutputPower(0b1111); // 20dbm = 100mW
#else
    // Below output power settings are for 1W modules
    // Radio.SetOutputPower(0b0000); // 15dbm = 32mW
    // Radio.SetOutputPower(0b0001); // 18dbm = 40mW
    // Radio.SetOutputPower(0b0101); // 20dbm = 100mW
    Radio.SetOutputPower(0b1000); // 23dbm = 200mW

    // Radio.SetOutputPower(0b1100); // 27dbm = 500mW
    // Radio.SetOutputPower(0b1111); // 30dbm = 1000mW
#endif // TARGET_100mW_MODULE
#elif defined Regulatory_Domain_AU_433 || defined Regulatory_Domain_EU_433
    DEBUG_PRINTLN("Setting 433MHz Mode");
    Radio.RFmodule = RFMOD_SX1278; //define radio module here
    Radio.SetOutputPower(0b1111);
#endif

    Radio.SetFrequency(GetInitialFreq()); //set frequency first or an error will occur!!!

    Radio.RXdoneCallback1 = &ProcessTLMpacket;

    Radio.TXdoneCallback1 = &HandleFHSS;
    Radio.TXdoneCallback2 = &HandleTLM;
    Radio.TXdoneCallback3 = &HandleUpdateParameter;
    //Radio.TXdoneCallback4 = &NULL;

#ifndef One_Bit_Switches
    crsf.RCdataCallback1 = &CheckChannels5to8Change;
#endif

    bool HighPower = false;
#ifdef TARGET_1000mW_MODULE
    HighPower = true;
#endif // TARGET_1000mW_MODULE
    Radio.Begin(HighPower);
    crsf.Begin();

    SetRFLinkRate(RATE_200HZ);
}

void loop()
{
    uint32_t current_ms = millis();

    //updateLEDs(isRXconnected, ExpressLRS_currAirRate->TLMinterval);

    if (current_ms > (LastTLMpacketRecvMillis + RX_CONNECTION_LOST_TIMEOUT))
    {
        isRXconnected = false;
    }
    else
    {
        isRXconnected = true;
    }
    platform_connection_state(isRXconnected);

    if (current_ms >= PacketRateNextCheck)
    {
        PacketRateNextCheck = current_ms + PACKET_RATE_INTERVAL;

        float targetFrameRate = ExpressLRS_currAirRate->rate;
        if (TLM_RATIO_NO_TLM != ExpressLRS_currAirRate->TLMinterval)
            targetFrameRate /= TLMratioEnumToValue(ExpressLRS_currAirRate->TLMinterval);
        PacketRate = (float)packetCounteRX_TX / (float)(PACKET_RATE_INTERVAL);
        linkQuality = int((((float)PacketRate / targetFrameRate) * 100000.0));
        if (linkQuality > 99)
        {
            linkQuality = 99;
        }
        packetCounteRX_TX = 0;
    }

    // Process CRSF packets from TX
    crsf.TX_handleUartIn();

    platform_loop(isRXconnected);
}

void ICACHE_RAM_ATTR TimerExpired()
{
    SendRCdataToRF();
}

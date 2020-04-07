#include <Arduino.h>
#include "FIFO.h"
#include "utils.h"
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"
#include "FHSS.h"
#include "targets.h"
#include "POWERMGNT.h"
#include "HwTimer.h"
#include "debug.h"

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT 1500U // After 1500ms of no TLM response consider that slave has lost connection
#define LQ_CALCULATE_INTERVAL 500u
#define SYNC_PACKET_SEND_INTERVAL_RX_LOST 250u  // how often to send the switch data packet (ms) when there is no response from RX
#define SYNC_PACKET_SEND_INTERVAL_RX_CONN 1500u // how often to send the switch data packet (ms) when there we have a connection
///////////////////

/// define some libs to use ///
SX127xDriver Radio;
CRSF crsf(CrsfSerial);
POWERMGNT PowerMgmt;

//// Switch Data Handling ///////
#if !defined(HYBRID_SWITCHES_8) && !defined(SEQ_SWITCHES)
uint32_t SwitchPacketNextSend = 0; //time in ms when the next switch data packet will be send
#define SWITCH_PACKET_SEND_INTERVAL 200u
#endif

/////////// SYNC PACKET ////////
static uint32_t SyncPacketNextSend = 0;

/////////// CONNECTION /////////
static volatile uint32_t LastTLMpacketRecvMillis = 0;
static volatile bool isRXconnected = false;

//////////// TELEMETRY /////////
static volatile uint32_t recv_tlm_counter = 0;
static volatile uint8_t downlink_linkQuality = 0;
static uint32_t PacketRateNextCheck = 0;
static volatile bool WaitRXresponse = false;

///////////////////////////////////////

static volatile bool UpdateParamReq = false;

static volatile bool Channels5to8Changed = false;

//static bool ChangeAirRateRequested = false;
//static bool ChangeAirRateSentUpdate = false;

///// Not used in this version /////////////////////////////////////////////////////////////////////////////////////////////////////////
//uint8_t TelemetryWaitBuffer[7] = {0};

//uint32_t LinkSpeedIncreaseDelayFactor = 500; // wait for the connection to be 'good' for this long before increasing the speed.
//uint32_t LinkSpeedDecreaseDelayFactor = 200; // this long wait this long for connection to be below threshold before dropping speed

//uint32_t LinkSpeedDecreaseFirstMetCondition = 0;
//uint32_t LinkSpeedIncreaseFirstMetCondition = 0;

//uint8_t LinkSpeedReduceSNR = 20;   //if the SNR (times 10) is lower than this we drop the link speed one level
//uint8_t LinkSpeedIncreaseSNR = 60; //if the SNR (times 10) is higher than this we increase the link speed
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

    recv_tlm_counter++;

    if (type != TLM_PACKET)
    {
        DEBUG_PRINTLN("TLM type error");
        DEBUG_PRINTLN(type);
    }

    isRXconnected = true;
    platform_connection_state(STATE_connected);
    LastTLMpacketRecvMillis = millis();

    if (TLMheader == CRSF_FRAMETYPE_LINK_STATISTICS)
    {
        crsf.LinkStatistics.uplink_RSSI_1 = Radio.RXdataBuffer[2];
        crsf.LinkStatistics.uplink_RSSI_2 = 0;
        crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
        crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];

        crsf.LinkStatistics.downlink_SNR = int(Radio.LastPacketSNR * 10);
        crsf.LinkStatistics.downlink_RSSI = 120 + Radio.LastPacketRSSI;
        crsf.LinkStatistics.downlink_Link_quality = downlink_linkQuality;
        //crsf.LinkStatistics.downlink_Link_quality = Radio.currPWR;
        crsf.LinkStatistics.rf_Mode = RATE_MAX - ExpressLRS_currAirRate->enum_rate; // 4 ??

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

#ifdef SEQ_SWITCHES
/**
 * Sequential switches packet
 * Replaces Generate4ChannelData_11bit
 * Channel 3 is reduced to 10 bits to allow a 3 bit switch index and 2 bit value
 * We cycle through 8 switches on successive packets. If any switches have changed
 * we take the lowest indexed one and send that, hence lower indexed switches have
 * higher priority in the event that several are changed at once.
 */
void ICACHE_RAM_ATTR GenerateChannelDataSeqSwitch()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = (DeviceAddr << 2) + RC_DATA_PACKET;
    Radio.TXdataBuffer[0] = PacketHeaderAddr;
    Radio.TXdataBuffer[1] = ((crsf.ChannelDataIn[0]) >> 3);
    Radio.TXdataBuffer[2] = ((crsf.ChannelDataIn[1]) >> 3);
    Radio.TXdataBuffer[3] = ((crsf.ChannelDataIn[2]) >> 3);
    Radio.TXdataBuffer[4] = ((crsf.ChannelDataIn[3]) >> 3);
    Radio.TXdataBuffer[5] = ((crsf.ChannelDataIn[0] & 0b00000111) << 5) + ((crsf.ChannelDataIn[1] & 0b111) << 2) + ((crsf.ChannelDataIn[2] & 0b110) >> 1);
    Radio.TXdataBuffer[6] = ((crsf.ChannelDataIn[2] & 0b001) << 7) + ((crsf.ChannelDataIn[3] & 0b110) << 4);

    // find the next switch to send
    int i = crsf.getNextSwitchIndex();
    // put the bits into buf[6]
    Radio.TXdataBuffer[6] += (i << 2) + crsf.currentSwitches[i];
}
#endif

#ifdef HYBRID_SWITCHES_8
/**
 * Hybrid switches packet
 * Replaces Generate4ChannelData_11bit
 * Analog channels are reduced to 10 bits to allow for switch encoding
 * Switch[0] is sent on every packet.
 * A 3 bit switch index and 2 bit value is used to send the remaining switches
 * in a round-robin fashion.
 * If any of the round-robin switches have changed
 * we take the lowest indexed one and send that, hence lower indexed switches have
 * higher priority in the event that several are changed at once.
 */
void ICACHE_RAM_ATTR GenerateChannelDataHybridSwitch8()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = (DeviceAddr << 2) + RC_DATA_PACKET;
    Radio.TXdataBuffer[0] = PacketHeaderAddr;
    Radio.TXdataBuffer[1] = ((crsf.ChannelDataIn[0]) >> 3);
    Radio.TXdataBuffer[2] = ((crsf.ChannelDataIn[1]) >> 3);
    Radio.TXdataBuffer[3] = ((crsf.ChannelDataIn[2]) >> 3);
    Radio.TXdataBuffer[4] = ((crsf.ChannelDataIn[3]) >> 3);
    Radio.TXdataBuffer[5] = ((crsf.ChannelDataIn[0] & 0b110) << 5) + ((crsf.ChannelDataIn[1] & 0b110) << 3) +
                            ((crsf.ChannelDataIn[2] & 0b110) << 1) + ((crsf.ChannelDataIn[3] & 0b110) >> 1);

    // switch 0 is sent on every packet - intended for low latency arm/disarm
    Radio.TXdataBuffer[6] = (crsf.currentSwitches[0] & 0b11) << 5; // note this leaves the top bit of byte 6 unused

    // find the next switch to send
    int i = crsf.getNextSwitchIndex() & 0b111; // mask for paranoia

    // put the bits into buf[6]. i is in the range 1 through 7 so takes 3 bits
    // currentSwitches[i] is in the range 0 through 2, takes 2 bits.
    Radio.TXdataBuffer[6] += (i << 2) + (crsf.currentSwitches[i] & 0b11); // mask for paranoia
}
#endif

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

void ICACHE_RAM_ATTR SetRFLinkRate(uint8_t rate) // Set speed of RF link (hz)
{
    const expresslrs_mod_settings_s *const config = get_elrs_airRateConfig(rate);
    if (config == ExpressLRS_currAirRate)
        return; // No need to modify, rate is same

//#ifdef PLATFORM_ESP32
#if 0
    Radio.TimerInterval = config->interval;
    Radio.UpdateTimerInterval();
#else
    TxTimer.updateInterval(config->interval); // TODO: Make sure this is equiv to above commented lines
#endif

    Radio.Config(config->bw, config->sf, config->cr, Radio.currFreq, Radio._syncWord);
    Radio.SetPreambleLength(config->PreambleLen);

    ExpressLRS_prevAirRate = ExpressLRS_currAirRate;
    ExpressLRS_currAirRate = config;

    crsf.RequestedRCpacketInterval = config->interval;

    // Set connected if telemetry is not used
    isRXconnected = (TLM_RATIO_NO_TLM == config->TLMinterval); // false;
    platform_connection_state((isRXconnected ? STATE_connected : STATE_disconnected));
#ifdef TARGET_R9M_TX
    //r9dac.resume();
#endif
}

uint8_t ICACHE_RAM_ATTR decRFLinkRate()
{
    DEBUG_PRINTLN("dec");
    if ((uint8_t)ExpressLRS_currAirRate->enum_rate < (RATE_MAX - 1))
    {
        SetRFLinkRate((ExpressLRS_currAirRate->enum_rate + 1));
    }
    return (uint8_t)ExpressLRS_currAirRate->enum_rate;
}

uint8_t ICACHE_RAM_ATTR incRFLinkRate()
{
    DEBUG_PRINTLN("inc");
    if ((uint8_t)ExpressLRS_currAirRate->enum_rate > RATE_200HZ)
    {
        SetRFLinkRate((ExpressLRS_currAirRate->enum_rate - 1));
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
    if (ExpressLRS_currAirRate->TLMinterval > TLM_RATIO_NO_TLM)
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
    uint32_t current_ms;
    uint8_t crc;

    DEBUG_PRINT("I");

#ifdef FEATURE_OPENTX_SYNC
    crsf.UpdateOpenTxSyncOffset(); // tells the crsf that we want to send data now - this allows opentx packet syncing
#endif

    /////// This Part Handles the Telemetry Response ///////
    if (ExpressLRS_currAirRate->TLMinterval > TLM_RATIO_NO_TLM)
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
                Radio.NonceTX++; // what for?
            }
        }
    }

    current_ms = millis();

    //only send sync when its time and only on channel 0;
    //if (((current_ms > SyncPacketNextSend) && (Radio.currFreq == GetInitialFreq())) || ChangeAirRateRequested)
    if ((current_ms > SyncPacketNextSend) && (Radio.currFreq == GetInitialFreq()))
    {
        GenerateSyncPacketData();
        SyncPacketNextSend = current_ms + ((isRXconnected) ? SYNC_PACKET_SEND_INTERVAL_RX_CONN : SYNC_PACKET_SEND_INTERVAL_RX_LOST);
        //ChangeAirRateSentUpdate = true;
        //DEBUG_PRINTLN("sync");
        //DEBUG_PRINTLN(Radio.currFreq);
    }
    else
    {
#if defined HYBRID_SWITCHES_8
        GenerateChannelDataHybridSwitch8();
#elif defined SEQ_SWITCHES
        GenerateChannelDataSeqSwitch();
#else
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
#endif
    }

    ///// Next, Calculate the CRC and put it into the buffer /////
    crc = CalcCRC(Radio.TXdataBuffer, 7) + CRCCaesarCipher;
    Radio.TXdataBuffer[7] = crc;
#ifdef TARGET_R9M_TX
    //r9dac.resume(); takes too long
    digitalWrite(GPIO_PIN_RFswitch_CONTROL, 0);
    digitalWrite(GPIO_PIN_RFamp_APC1, 1);
#endif
    Radio.TXnb(Radio.TXdataBuffer, 8);

    /*if (ChangeAirRateRequested)
    {
        ChangeAirRateSentUpdate = true;
    }*/
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
        break;
    case 1:
        if (crsf.ParameterUpdateData[1] == 0)
        {
            /*uint8_t newRate =*/decRFLinkRate();
        }
        else if (crsf.ParameterUpdateData[1] == 1)
        {
            /*uint8_t newRate =*/incRFLinkRate();
        }
        DEBUG_PRINTLN(ExpressLRS_currAirRate->enum_rate);
        break;

    case 2:

        break;
    case 3:

        if (crsf.ParameterUpdateData[1] == 0)
        {
            PowerMgmt.decPower();
        }
        else if (crsf.ParameterUpdateData[1] == 1)
        {
            PowerMgmt.incPower();
        }

        break;
    case 4:

        break;

    default:
        break;
    }

    UpdateParamReq = false;
    //DEBUG_PRINTLN("Power");
    //DEBUG_PRINTLN(PowerMgmt.currPower());
    crsf.sendLUAresponse((ExpressLRS_currAirRate->enum_rate + 2),
                         ExpressLRS_currAirRate->TLMinterval + 1,
                         PowerMgmt.currPower() + 2,
                         4);
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

static void hw_timer_init(void)
{
    TxTimer.init();
}
static void hw_timer_stop(void)
{
    TxTimer.stop();
}

void setup()
{
    CrsfSerial.Begin(CRSF_OPENTX_BAUDRATE);

    platform_setup();

    crsf.connected = hw_timer_init; // it will auto init when it detects UART connection
    crsf.disconnected = hw_timer_stop;
    crsf.RecvParameterUpdate = &ParamUpdateReq;
    TxTimer.callbackTock = &SendRCdataToRF;

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
#endif

    Radio.RXdoneCallback1 = &ProcessTLMpacket;

    Radio.TXdoneCallback1 = &HandleFHSS;
    Radio.TXdoneCallback2 = &HandleTLM;
    Radio.TXdoneCallback3 = &HandleUpdateParameter;
    //Radio.TXdoneCallback4 = &NULL;

#ifndef One_Bit_Switches
    crsf.RCdataCallback1 = &CheckChannels5to8Change;
#endif

    PowerMgmt.defaultPower();
    Radio.SetFrequency(GetInitialFreq()); //set frequency first or an error will occur!!!

    bool HighPower = false;
#ifdef TARGET_1000mW_MODULE
    HighPower = true;
#endif // TARGET_1000mW_MODULE
    Radio.Begin(HighPower);
    crsf.Begin();

    SetRFLinkRate(RATE_DEFAULT);

    platform_connection_state(STATE_disconnected);
}

void loop()
{
    if (TLM_RATIO_NO_TLM != ExpressLRS_currAirRate->TLMinterval)
    {
        uint32_t current_ms = millis();

        if (current_ms > (LastTLMpacketRecvMillis + RX_CONNECTION_LOST_TIMEOUT))
        {
            isRXconnected = false;
            platform_connection_state(STATE_disconnected);
        }

        if (current_ms >= PacketRateNextCheck)
        {
            PacketRateNextCheck = current_ms + LQ_CALCULATE_INTERVAL;

#if 0
        // TODO: this is not ok... need improvements
        float targetFrameRate = ExpressLRS_currAirRate->rate; // 200
        if (TLM_RATIO_NO_TLM != ExpressLRS_currAirRate->TLMinterval)
            targetFrameRate /= TLMratioEnumToValue(ExpressLRS_currAirRate->TLMinterval); //  / 64
        float PacketRate = recv_tlm_counter;
        PacketRate /= LQ_CALCULATE_INTERVAL; // num tlm packets
        downlink_linkQuality = int(((PacketRate / targetFrameRate) * 100000.0));
        if (downlink_linkQuality > 99)
        {
            downlink_linkQuality = 99;
        }
        recv_tlm_counter = 0;
#else
            // Calc LQ based on good tlm packets and receptions done
            uint8_t rx_cnt = Radio.NonceRX;
            uint32_t tlm_cnt = recv_tlm_counter;
            Radio.NonceRX = recv_tlm_counter = 0; // Clear RX counter
            if (rx_cnt)
                downlink_linkQuality = (tlm_cnt * 100u) / rx_cnt;
            else
                // failure value??
                downlink_linkQuality = 0;
#endif
        }
    }

    // Process CRSF packets from TX
    crsf.TX_handleUartIn();

    platform_loop((isRXconnected ? STATE_connected : STATE_disconnected));
}

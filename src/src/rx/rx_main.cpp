#include <Arduino.h>
#include "targets.h"
#include "utils.h"
#include "common.h"
#include "LowPassFilter.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"
#include "FHSS.h"
#include "rx_LinkQuality.h"
#include "errata.h"
#include "HwTimer.h"
#include "HwSerial.h"
#include "debug.h"

//// CONSTANTS ////
#define SEND_LINK_STATS_TO_FC_INTERVAL 100

///////////////////

CRSF crsf(CrsfSerial); //pass a serial port object to the class for it to use

volatile connectionState_e connectionState = STATE_disconnected;

/// Filters ////////////////
static LPF LPF_PacketInterval(3);
static LPF LPF_Offset(3);
static LPF LPF_FreqError(3);
static LPF LPF_UplinkRSSI(5);
////////////////////////////

static uint8_t scanIndex = 0;

///////////////////////////////////////////////

static volatile uint8_t NonceRXlocal = 0; // nonce that we THINK we are up to.

static volatile bool alreadyFHSS = false;

//////////////////////////////////////////////////////////////
////// Variables for Telemetry and Link Quality //////////////

//volatile uint32_t LastValidPacketMicros = 0;
//volatile uint32_t LastValidPacketPrevMicros = 0; //Previous to the last valid packet (used to measure the packet interval)
static volatile uint32_t LastValidPacket = 0; //Time the last valid packet was recv

static uint32_t SendLinkStatstoFCintervalNextSend = SEND_LINK_STATS_TO_FC_INTERVAL;

///////////////////////////////////////////////////////////////
///////////// Variables for Sync Behaviour ////////////////////
static volatile uint32_t RFmodeNextCycle = 0;

static inline void RfModeNextCycleCalc(void)
{
    // if connection == tentative, add a little delay
    RFmodeNextCycle = millis() +
                      ExpressLRS_currAirRate->RFmodeCycleInterval +
                      ((connectionState == STATE_tentative) ? ExpressLRS_currAirRate->RFmodeCycleAddtionalTime : 0);
}

static inline void TimerAdjustment(uint32_t us)
{
    /* Adjust timer only if sync ok */
    uint32_t interval = ExpressLRS_currAirRate->interval;
    us -= TxTimer.LastCallbackMicrosTick;
    int32_t HWtimerError = us % interval;
    HWtimerError -= (interval >> 1);
    int32_t Offset = LPF_Offset.update(HWtimerError); //crude 'locking function' to lock hardware timer to transmitter, seems to work well enough
    TxTimer.phaseShift(uint32_t((Offset >> 4) + timerOffset));
}

///////////////////////////////////////

void ICACHE_RAM_ATTR getRFlinkInfo()
{
    int8_t LastRSSI = Radio.LastPacketRSSI;
    crsf.PackedRCdataOut.ch15 = UINT10_to_CRSF(map(LastRSSI, -100, -50, 0, 1023));
    crsf.PackedRCdataOut.ch14 = UINT10_to_CRSF(fmap(linkQuality, 0, 100, 0, 1023));
#if 0
    int32_t rssiDBM = LPF_UplinkRSSI.update(LastRSSI);
    // our rssiDBM is currently in the range -128 to 98, but BF wants a value in the range
    // 0 to 255 that maps to -1 * the negative part of the rssiDBM, so cap at 0.
    if (rssiDBM > 0)
        rssiDBM = 0;
    crsf.LinkStatistics.uplink_RSSI_1 = -1 * rssiDBM; // to match BF
#else
    //DEBUG_PRINT(Radio.LastPacketRssiRaw);
    uint8_t rssi = LPF_UplinkRSSI.update(Radio.LastPacketRssiRaw);
    crsf.LinkStatistics.uplink_RSSI_1 = (rssi > 127) ? 127 : rssi;
#endif
    //crsf.LinkStatistics.uplink_RSSI_2 = 0;
    crsf.LinkStatistics.uplink_SNR = Radio.LastPacketSNR * 10;
    crsf.LinkStatistics.uplink_Link_quality = linkQuality;
    //crsf.LinkStatistics.rf_Mode = 4 - ExpressLRS_currAirRate->enum_rate;
    //DEBUG_PRINTLN(crsf.LinkStatistics.uplink_RSSI_1);
}

void ICACHE_RAM_ATTR HandleFHSS()
{
    if (connectionState > STATE_disconnected) // don't hop if we lost
    {
        uint8_t modresult = (NonceRXlocal + 1) % ExpressLRS_currAirRate->FHSShopInterval;
        if (modresult != 0)
        {
            return;
        }

        linkQuality = getRFlinkQuality();
        Radio.SetFrequency(FHSSgetNextFreq()); // 220us => 85us
        Radio.RXnb();                          // 260us => 148us
    }
}

void ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    if (/*connectionState != STATE_connected ||*/
        ExpressLRS_currAirRate->TLMinterval == TLM_RATIO_NO_TLM)
    {
        // don't bother sending tlm if disconnected or no tlm enabled
        return;
    }

    // Check if tlm time
    uint8_t modresult = (NonceRXlocal + 1) % TLMratioEnumToValue(ExpressLRS_currAirRate->TLMinterval);
    if (modresult != 0)
    {
        return;
    }

    Radio.TXdataBuffer[0] = (DeviceAddr << 2) + 0b11; // address + tlm packet
    Radio.TXdataBuffer[1] = CRSF_FRAMETYPE_LINK_STATISTICS;

    // OpenTX hard codes "rssi" warnings to the LQ sensor for crossfire, so the
    // rssi we send is for display only.
    // OpenTX treats the rssi values as signed.
#if 0
    uint8_t openTxRSSI = crsf.LinkStatistics.uplink_RSSI_1;
    // truncate the range to fit into OpenTX's 8 bit signed value
    if (openTxRSSI > 127)
        openTxRSSI = 127;
    // convert to 8 bit signed value in the negative range (-128 to 0)
    openTxRSSI = 255 - openTxRSSI;
    Radio.TXdataBuffer[2] = openTxRSSI;
#else
    Radio.TXdataBuffer[2] = crsf.LinkStatistics.uplink_RSSI_1;
#endif
    Radio.TXdataBuffer[3] = (crsf.TLMbattSensor.voltage & 0xFF00) >> 8;
    Radio.TXdataBuffer[4] = crsf.LinkStatistics.uplink_SNR;
    Radio.TXdataBuffer[5] = crsf.LinkStatistics.uplink_Link_quality;
    Radio.TXdataBuffer[6] = (crsf.TLMbattSensor.voltage & 0x00FF);

    uint8_t crc = CalcCRC(Radio.TXdataBuffer, 7) + CRCCaesarCipher;
    Radio.TXdataBuffer[7] = crc;
    Radio.TXnb(Radio.TXdataBuffer, 8);
    addPacketToLQ(); // Adds packet to LQ otherwise an artificial drop in LQ is seen due to sending TLM.
}

void ICACHE_RAM_ATTR HWtimerCallback()
{
#if 0
    if (alreadyFHSS == true)
    {
        alreadyFHSS = false;
    }
    else
    {
        HandleFHSS();
    }
#else
    HandleFHSS();
#endif

    incrementLQArray();
    HandleSendTelemetryResponse();

    NonceRXlocal++;
}

void LostConnection()
{
    if (connectionState <= STATE_disconnected)
    {
        return; // Already disconnected
    }

    connectionState = STATE_disconnected; //set lost connection
    LPF_FreqError.init(0);

    led_set_state(1);                     // turn off led
    Radio.SetFrequency(GetInitialFreq()); // in conn lost state we always want to listen on freq index 0
    DEBUG_PRINTLN("lost conn");

    platform_connection_state(connectionState);
}

void ICACHE_RAM_ATTR TentativeConnection()
{
    connectionState = STATE_tentative;
    DEBUG_PRINTLN("tentative conn");
}

void ICACHE_RAM_ATTR GotConnection()
{
    //if (connectionState == STATE_connected)
    if (connectionState != STATE_tentative)
    {
        return; // Already connected
    }

    //TxTimer.start(); // start tlm timer

    connectionState = STATE_connected; //we got a packet, therefore no lost connection
    scanIndex = 0;
    //RfModeNextCycleCalc(); // give another 3 sec for loc to occur.

    led_set_state(0); // turn on led
    DEBUG_PRINTLN("got conn");

    platform_connection_state(connectionState);
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

#ifdef SEQ_SWITCHES
/**
 * Seq switches uses 10 bits for ch3, 3 bits for the switch index and 2 bits for the switch value
 */
void ICACHE_RAM_ATTR UnpackChannelDataSeqSwitches()
{
    crsf.PackedRCdataOut.ch0 = (Radio.RXdataBuffer[1] << 3) + ((Radio.RXdataBuffer[5] & 0b11100000) >> 5);
    crsf.PackedRCdataOut.ch1 = (Radio.RXdataBuffer[2] << 3) + ((Radio.RXdataBuffer[5] & 0b00011100) >> 2);
    crsf.PackedRCdataOut.ch2 = (Radio.RXdataBuffer[3] << 3) + ((Radio.RXdataBuffer[5] & 0b00000011) << 1) + (Radio.RXdataBuffer[6] & 0b10000000 >> 7);
    crsf.PackedRCdataOut.ch3 = (Radio.RXdataBuffer[4] << 3) + ((Radio.RXdataBuffer[6] & 0b01100000) >> 4);

    uint8_t switchIndex = (Radio.RXdataBuffer[6] & 0b11100) >> 2;
    uint16_t switchValue = SWITCH2b_to_CRSF(Radio.RXdataBuffer[6] & 0b11);

    switch (switchIndex)
    {
    case 0:
        crsf.PackedRCdataOut.ch4 = switchValue;
        break;
    case 1:
        crsf.PackedRCdataOut.ch5 = switchValue;
        break;
    case 2:
        crsf.PackedRCdataOut.ch6 = switchValue;
        break;
    case 3:
        crsf.PackedRCdataOut.ch7 = switchValue;
        break;
    case 4:
        crsf.PackedRCdataOut.ch8 = switchValue;
        break;
    case 5:
        crsf.PackedRCdataOut.ch9 = switchValue;
        break;
    case 6:
        crsf.PackedRCdataOut.ch10 = switchValue;
        break;
    case 7:
        crsf.PackedRCdataOut.ch11 = switchValue;
        break;
    }
}

#elif defined(HYBRID_SWITCHES_8)
/**
 * Hybrid switches uses 10 bits for each analog channel,
 * 2 bits for the low latency switch[0]
 * 3 bits for the round-robin switch index and 2 bits for the value
 */
void ICACHE_RAM_ATTR UnpackChannelDataHybridSwitches8()
{
    // The analog channels
    crsf.PackedRCdataOut.ch0 = (Radio.RXdataBuffer[1] << 3) + ((Radio.RXdataBuffer[5] & 0b11000000) >> 5);
    crsf.PackedRCdataOut.ch1 = (Radio.RXdataBuffer[2] << 3) + ((Radio.RXdataBuffer[5] & 0b00110000) >> 3);
    crsf.PackedRCdataOut.ch2 = (Radio.RXdataBuffer[3] << 3) + ((Radio.RXdataBuffer[5] & 0b00001100) >> 1);
    crsf.PackedRCdataOut.ch3 = (Radio.RXdataBuffer[4] << 3) + ((Radio.RXdataBuffer[5] & 0b00000011) << 1);

    // The low latency switch
    crsf.PackedRCdataOut.ch4 = SWITCH2b_to_CRSF((Radio.RXdataBuffer[6] & 0b01100000) >> 5);

    // The round-robin switch
    uint8_t switchIndex = (Radio.RXdataBuffer[6] & 0b11100) >> 2;
    uint16_t switchValue = SWITCH2b_to_CRSF(Radio.RXdataBuffer[6] & 0b11);

    switch (switchIndex)
    {
    case 0: // we should never get index 0 here since that is the low latency switch
        Serial.println("BAD switchIndex 0");
        break;
    case 1:
        crsf.PackedRCdataOut.ch5 = switchValue;
        break;
    case 2:
        crsf.PackedRCdataOut.ch6 = switchValue;
        break;
    case 3:
        crsf.PackedRCdataOut.ch7 = switchValue;
        break;
    case 4:
        crsf.PackedRCdataOut.ch8 = switchValue;
        break;
    case 5:
        crsf.PackedRCdataOut.ch9 = switchValue;
        break;
    case 6:
        crsf.PackedRCdataOut.ch10 = switchValue;
        break;
    case 7:
        crsf.PackedRCdataOut.ch11 = switchValue;
        break;
    }
}
#endif

#if 0
void ICACHE_RAM_ATTR UnpackChannelData_10bit()
{
    crsf.PackedRCdataOut.ch0 = UINT10_to_CRSF((Radio.RXdataBuffer[1] << 2) + ((Radio.RXdataBuffer[5] & 0b11000000) >> 6));
    crsf.PackedRCdataOut.ch1 = UINT10_to_CRSF((Radio.RXdataBuffer[2] << 2) + ((Radio.RXdataBuffer[5] & 0b00110000) >> 4));
    crsf.PackedRCdataOut.ch2 = UINT10_to_CRSF((Radio.RXdataBuffer[3] << 2) + ((Radio.RXdataBuffer[5] & 0b00001100) >> 2));
    crsf.PackedRCdataOut.ch3 = UINT10_to_CRSF((Radio.RXdataBuffer[4] << 2) + ((Radio.RXdataBuffer[5] & 0b00000011) >> 0));
}
#endif

#if !defined(SEQ_SWITCHES) && !defined(HYBRID_SWITCHES_8)
void ICACHE_RAM_ATTR UnpackSwitchData()
{
    crsf.PackedRCdataOut.ch4 = SWITCH3b_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b11100000) >> 5); //unpack the byte structure, each switch is stored as a possible 8 states (3 bits). we shift by 2 to translate it into the 0....1024 range like the other channel data.
    crsf.PackedRCdataOut.ch5 = SWITCH3b_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b00011100) >> 2);
    crsf.PackedRCdataOut.ch6 = SWITCH3b_to_CRSF((uint16_t)((Radio.RXdataBuffer[1] & 0b00000011) << 1) + ((Radio.RXdataBuffer[2] & 0b10000000) >> 7));
    crsf.PackedRCdataOut.ch7 = SWITCH3b_to_CRSF((uint16_t)((Radio.RXdataBuffer[2] & 0b01110000) >> 4));
}
#endif

void ICACHE_RAM_ATTR ProcessRFPacket()
{
    DEBUG_PRINT("I");
    uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
    uint8_t inCRC = Radio.RXdataBuffer[7];
    uint8_t type = Radio.RXdataBuffer[0] & 0b11;
    uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;

    if (inCRC != calculatedCRC)
    {
        //DEBUG_PRINTLN("CRC error on RF packet");
        DEBUG_PRINT("!CRC");
        return;
    }

    if (packetAddr != DeviceAddr)
    {
        //DEBUG_PRINTLN("Wrong device address on RF packet");
        DEBUG_PRINT("!ADDR");
        return;
    }

    TimerAdjustment(micros()); // Adjust timer phase

    //LastValidPacketPrevMicros = LastValidPacketMicros;
    //LastValidPacketMicros = micros();
    LastValidPacket = millis();

    getRFlinkInfo();

    switch (type)
    {
    case RC_DATA_PACKET: //Standard RC Data Packet
#if defined(SEQ_SWITCHES)
        UnpackChannelDataSeqSwitches();
#elif defined(HYBRID_SWITCHES_8)
        UnpackChannelDataHybridSwitches8();
#else
        UnpackChannelData_11bit();
#endif
        crsf.sendRCFrameToFC();
        break;

#if !defined(SEQ_SWITCHES) && !defined(HYBRID_SWITCHES_8)
    case SWITCH_DATA_PACKET:                                                                                      // Switch Data Packet
        if ((Radio.RXdataBuffer[3] == Radio.RXdataBuffer[1]) && (Radio.RXdataBuffer[4] == Radio.RXdataBuffer[2])) // extra layer of protection incase the crc and addr headers fail us.
        {
            UnpackSwitchData();
            NonceRXlocal = Radio.RXdataBuffer[5];
            FHSSsetCurrIndex(Radio.RXdataBuffer[6]);
            crsf.sendRCFrameToFC();
        }
        break;
#endif

    case TLM_PACKET: //telemetry packet from master
        // not implimented yet
        break;

    case SYNC_PACKET: //sync packet from master
        if (Radio.RXdataBuffer[4] == UID[3] && Radio.RXdataBuffer[5] == UID[4] && Radio.RXdataBuffer[6] == UID[5])
        {
            if (connectionState == STATE_disconnected)
            {
                TentativeConnection();
            }

            if (connectionState == STATE_tentative &&
                NonceRXlocal == Radio.RXdataBuffer[2] &&
                FHSSgetCurrIndex() == Radio.RXdataBuffer[1])
            {
                GotConnection();
            }

            // if (ExpressLRS_currAirRate->enum_rate == !(uint8_t)(Radio.RXdataBuffer[2] & 0b00001111))
            // {
            //     DEBUG_PRINTLN("update air rate");
            //     SetRFLinkRate(Radio.RXdataBuffer[3]);
            // }

            FHSSsetCurrIndex(Radio.RXdataBuffer[1]);
            NonceRXlocal = Radio.RXdataBuffer[2];
        }
        break;

    default: // code to be executed if n doesn't match any cases
        break;
    }

    addPacketToLQ();

#if 0
    if (connectionState >= STATE_tentative)
    {
        /* Adjust timer only if sync ok */
        int32_t HWtimerError = ((micros() - TxTimer.LastCallbackMicrosTick) % ExpressLRS_currAirRate->interval);
        int32_t Offset = LPF_Offset.update(HWtimerError - (ExpressLRS_currAirRate->interval >> 1)); //crude 'locking function' to lock hardware timer to transmitter, seems to work well enough
        TxTimer.phaseShift(uint32_t((Offset >> 4) + timerOffset));
    }
#else
    //TimerAdjustment(micros());
#endif

    if (((NonceRXlocal + 1) % ExpressLRS_currAirRate->FHSShopInterval) == 0) //premept the FHSS if we already know we'll have to do it next timer tick.
    {
        int32_t freqerror = LPF_FreqError.update(Radio.GetFrequencyError()); // get freq error = 90us
        //DEBUG_PRINT(freqerror);
        //DEBUG_PRINT(" : ");

        if (freqerror > 0)
        {
            if (FreqCorrection < FreqCorrectionMax)
            {
                FreqCorrection += FreqCorrectionStep;
            }
            else
            {
                FreqCorrection = FreqCorrectionMax;
                //DEBUG_PRINTLN("Max pos reasontable freq offset correction limit reached!");
                DEBUG_PRINT("FREQ_MAX");
            }
        }
        else
        {
            if (FreqCorrection > FreqCorrectionMin)
            {
                FreqCorrection -= FreqCorrectionStep;
            }
            else
            {
                FreqCorrection = FreqCorrectionMin;
                //DEBUG_PRINTLN("Max neg reasontable freq offset correction limit reached!");
                DEBUG_PRINT("FREQ_MIN");
            }
        }

        Radio.setPPMoffsetReg(FreqCorrection);
        //DEBUG_PRINTLN(FreqCorrection);

        //HandleFHSS();
        //alreadyFHSS = true;
    }
    DEBUG_PRINT("E");
}

void forced_start(void)
{
    DEBUG_PRINTLN("Manual Start");
    Radio.SetFrequency(GetInitialFreq());
    Radio.RXnb();
}

void forced_stop(void)
{
    Radio.StopContRX();
    TxTimer.stop();
}

void SetRFLinkRate(uint8_t rate) // Set speed of RF link (hz)
{
    const expresslrs_mod_settings_s *const config = get_elrs_airRateConfig(rate);
    if (config == ExpressLRS_currAirRate)
        return; // No need to modify, rate is same

    Radio.StopContRX();
    Radio.Config(config->bw, config->sf, config->cr, Radio.currFreq, Radio._syncWord);
    ExpressLRS_currAirRate = config;
    TxTimer.updateInterval(config->interval);
    LPF_PacketInterval.init(config->interval);
    crsf.LinkStatistics.uplink_RSSI_2 = 0;
    crsf.LinkStatistics.rf_Mode = 4 - config->enum_rate;
    //LPF_Offset.init(0);
    //InitHarwareTimer();
    Radio.RXnb();
}

void tx_done_cb(void)
{
    Radio.RXnb();
}

void setup()
{
    platform_setup();

    CrsfSerial.Begin(CRSF_RX_BAUDRATE);
    // CrsfSerial.Begin(230400); // for linux debugging

    DEBUG_PRINTLN("Module Booting...");

#ifdef Regulatory_Domain_AU_915
    DEBUG_PRINTLN("Setting 915MHz Mode");
    Radio.RFmodule = RFMOD_SX1276; //define radio module here
#elif defined Regulatory_Domain_FCC_915
    DEBUG_PRINTLN("Setting 915MHz Mode");
    Radio.RFmodule = RFMOD_SX1276; //define radio module here
#elif defined Regulatory_Domain_EU_868
    DEBUG_PRINTLN("Setting 868MHz Mode");
    Radio.RFmodule = RFMOD_SX1276; //define radio module here
#elif defined Regulatory_Domain_AU_433 || defined Regulatory_Domain_EU_433
    DEBUG_PRINTLN("Setting 433MHz Mode");
    Radio.RFmodule = RFMOD_SX1278; //define radio module here
#else
#error No regulatory domain defined, please define one in common.h
#endif

    Radio.SetPins(GPIO_PIN_RST, GPIO_PIN_DIO0, GPIO_PIN_DIO1);
    Radio.Begin(false);

    FHSSrandomiseFHSSsequence();
    Radio.SetFrequency(GetInitialFreq());

    //Radio.SetSyncWord(0x122);
    Radio.SetOutputPower(0b1111); //default is max power (17dBm for RX)

    int16_t RFnoiseFloor = MeasureNoiseFloor();
    DEBUG_PRINT("RF noise floor: ");
    DEBUG_PRINT(RFnoiseFloor);
    DEBUG_PRINTLN("dBm");
    (void)RFnoiseFloor;

    Radio.RXdoneCallback1 = &ProcessRFPacket;

    Radio.TXdoneCallback1 = &tx_done_cb;

    TxTimer.callbackTock = &HWtimerCallback;
    TxTimer.init();
    //TxTimer.start(); // start tlm timer

    SetRFLinkRate(RATE_DEFAULT);

    crsf.Begin();
}

void loop()
{
    uint32_t now = millis();

    if (connectionState == STATE_disconnected)
    {
        if (now > RFmodeNextCycle)
        {
            Radio.SetFrequency(GetInitialFreq());
            SetRFLinkRate((scanIndex % RATE_MAX)); //switch between rates
            LQreset();
            led_toggle();
            //DEBUG_PRINTLN(ExpressLRS_currAirRate->interval);
            scanIndex++;

            RfModeNextCycleCalc();
        }
    }
    else if (connectionState > STATE_disconnected)
    {
        // check if we lost conn.
        if (now > (LastValidPacket + ExpressLRS_currAirRate->RFmodeCycleAddtionalTime))
        {
            LostConnection();
        }
        else if (now > SendLinkStatstoFCintervalNextSend)
        {
            crsf.sendLinkStatisticsToFC();
            SendLinkStatstoFCintervalNextSend = now + SEND_LINK_STATS_TO_FC_INTERVAL;
        }
    }

    crsf.RX_handleUartIn();

    platform_loop(connectionState);
}

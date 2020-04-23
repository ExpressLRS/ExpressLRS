#include <Arduino.h>
#include "targets.h"
#include "utils.h"
#include "common.h"
#include "LowPassFilter.h"
#include "LoRaRadioLib.h"
#include "CRSF_RX.h"
#include "FHSS.h"
#include "rx_LinkQuality.h"
#include "HwTimer.h"
#include "HwSerial.h"
#include "debug.h"
#include "helpers.h"
#include "rc_channels.h"

static void SetRFLinkRate(uint8_t rate);

//// CONSTANTS ////
#define SEND_LINK_STATS_TO_FC_INTERVAL 100
#define FHSS_ONLY_TIMER                0

///////////////////

CRSF_RX crsf(CrsfSerial); //pass a serial port object to the class for it to use
RcChannels rc_ch;

volatile connectionState_e connectionState = STATE_disconnected;
static volatile uint8_t NonceRXlocal = 0; // nonce that we THINK we are up to.
#if !FHSS_ONLY_TIMER
static volatile bool alreadyFHSS = false;
#endif
static volatile uint32_t tlm_check_ratio = 0;
static volatile uint8_t rx_buffer[8] __attribute__((aligned(32)));
static volatile uint8_t rx_buffer_handle = 0;

///////////////////////////////////////////////
////////////////  Filters  ////////////////////
static LPF LPF_Offset(3);
static LPF LPF_FreqError(5);
static LPF LPF_UplinkRSSI(5);

//////////////////////////////////////////////////////////////
////// Variables for Telemetry and Link Quality //////////////

static volatile uint32_t LastValidPacket = 0; //Time the last valid packet was recv
static uint32_t SendLinkStatstoFCintervalNextSend = SEND_LINK_STATS_TO_FC_INTERVAL;

///////////////////////////////////////////////////////////////
///////////// Variables for Sync Behaviour ////////////////////
static uint32_t RFmodeNextCycle = 0;
static volatile uint8_t scanIndex = 0;
static volatile uint8_t tentative_cnt = 0;

///////////////////////////////////////

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
    crsf.ChannelsPacked.ch15 = UINT10_to_CRSF(MAP(LastRSSI, -100, -50, 0, 1023));
    crsf.ChannelsPacked.ch14 = UINT10_to_CRSF(MAP_U16(linkQuality, 0, 100, 0, 1023));
#if 1
    int32_t rssiDBM = LPF_UplinkRSSI.update(LastRSSI);
    // our rssiDBM is currently in the range -128 to 98, but BF wants a value in the range
    // 0 to 255 that maps to -1 * the negative part of the rssiDBM, so cap at 0.
    if (rssiDBM > 0)
        rssiDBM = 0;
    crsf.LinkStatistics.uplink_RSSI_1 = -1 * rssiDBM; // to match BF
#else
    uint8_t rssi = LPF_UplinkRSSI.update(Radio.LastPacketRssiRaw);
    crsf.LinkStatistics.uplink_RSSI_1 = (rssi > 127) ? 127 : rssi;
#endif
    crsf.LinkStatistics.uplink_SNR = Radio.LastPacketSNR * 10;
    crsf.LinkStatistics.uplink_Link_quality = linkQuality;
    //DEBUG_PRINTLN(crsf.LinkStatistics.uplink_RSSI_1);
}

void ICACHE_RAM_ATTR HandleFHSS()
{
    if (connectionState > STATE_disconnected) // don't hop if we lost
    {
        if ((NonceRXlocal % ExpressLRS_currAirRate->FHSShopInterval) == 0)
        {
            //DEBUG_PRINT("F");
            linkQuality = LQ_getlinkQuality();
            Radio.RXnb(FHSSgetNextFreq()); // 260us => 148us => ??
        }
        NonceRXlocal++;
    }
}

void ICACHE_RAM_ATTR HandleSendTelemetryResponse()
{
    //DEBUG_PRINT("T");

    uint32_t __tx_buffer[2]; // esp requires aligned buffer
    uint8_t *tx_buffer = (uint8_t *)__tx_buffer;

    tx_buffer[0] = DEIVCE_ADDR_GENERATE(DeviceAddr) + TLM_PACKET; // address + tlm packet

    crsf.LinkStatisticsPack(&tx_buffer[1]);

    uint8_t crc = CalcCRC(tx_buffer, 7) + CRCCaesarCipher;
    tx_buffer[7] = crc;
    Radio.TXnb(tx_buffer, 8, FHSSgetCurrFreq());
    LQ_setPacketState(); // Adds packet to LQ otherwise an artificial drop in LQ is seen due to sending TLM.
}

void ICACHE_RAM_ATTR HWtimerCallback(uint32_t us)
{
    //DEBUG_PRINT("H");
#if !FHSS_ONLY_TIMER
    if (!alreadyFHSS)
#endif
    {
        HandleFHSS();
    }
#if !FHSS_ONLY_TIMER
    alreadyFHSS = false;
#endif

    // Check if connected before sending tlm resp
    if (STATE_disconnected < connectionState)
    {
        LQ_nextPacket();
        if (tlm_check_ratio && (NonceRXlocal & tlm_check_ratio) == 0)
        {
            HandleSendTelemetryResponse();
        }
    }
}

void ICACHE_RAM_ATTR LostConnection()
{
    if (connectionState <= STATE_disconnected)
    {
        return; // Already disconnected
    }

    connectionState = STATE_disconnected; //set lost connection
    scanIndex = RATE_DEFAULT;

    led_set_state(1);                     // turn off led
    Radio.SetFrequency(GetInitialFreq()); // in conn lost state we always want to listen on freq index 0
    DEBUG_PRINTLN("lost conn");

    platform_connection_state(connectionState);
}

void ICACHE_RAM_ATTR TentativeConnection()
{
    tentative_cnt = 0;
    connectionState = STATE_tentative;
    DEBUG_PRINTLN("tentative conn");
}

void ICACHE_RAM_ATTR GotConnection()
{
    connectionState = STATE_connected; //we got a packet, therefore no lost connection

    led_set_state(0); // turn on led
    DEBUG_PRINTLN("got conn");

    platform_connection_state(connectionState);
}

void process_rx_packet(void)
{
    switch (TYPE_GET(rx_buffer[0]))
    {
        case RC_DATA_PACKET: //Standard RC Data Packet
            if (connectionState == STATE_connected)
            {
                rc_ch.channels_extract(rx_buffer, crsf.ChannelsPacked);
                crsf.sendRCFrameToFC();
            }
            break;

#if !defined(SEQ_SWITCHES) && !defined(HYBRID_SWITCHES_8)
        case SWITCH_DATA_PACKET: // Switch Data Packet
            // extra layer of protection incase the crc and addr headers fail us.
            if ((rx_buffer[3] == rx_buffer[1]) &&
                (rx_buffer[4] == rx_buffer[2]))
            {
                if (connectionState == STATE_connected)
                {
                    rc_ch.channels_extract(rx_buffer, crsf.ChannelsPacked);
                    crsf.sendRCFrameToFC();
                }
                //NonceRXlocal = rx_buffer[5];
                //FHSSsetCurrIndex(rx_buffer[6]);
            }
            break;
#endif

        case TLM_PACKET: // telemetry packet
            break;

        case SYNC_PACKET:
            // sync packet is handled in rx isr
        default:
            break;
    }

    getRFlinkInfo();
}

void ICACHE_RAM_ATTR ProcessRFPacketCallback(uint8_t *buff)
{
    //DEBUG_PRINT("I");
    uint32_t current_us = micros();

    volatile_memcpy(rx_buffer, buff, sizeof(rx_buffer));

    uint8_t calculatedCRC = CalcCRC(rx_buffer, 7) + CRCCaesarCipher;
    uint8_t address = rx_buffer[0];

    if (rx_buffer[7] != calculatedCRC)
    {
        //DEBUG_PRINTLN("CRC error on RF packet");
        //DEBUG_PRINT("!CRC");
        DEBUG_PRINT("!C");
        return;
    }
    else if (DEIVCE_ADDR_GET(address) != DeviceAddr)
    {
        //DEBUG_PRINTLN("Wrong device address on RF packet");
        DEBUG_PRINT("!A");
        return;
    }

    //TimerAdjustment(Radio.LastPacketIsrMicros);
    TimerAdjustment(current_us);

    LQ_setPacketState(1);

    if (TYPE_GET(address) == SYNC_PACKET)
    {
        if (rx_buffer[4] == UID[3] &&
            rx_buffer[5] == UID[4] &&
            rx_buffer[6] == UID[5])
        {
            if (connectionState == STATE_disconnected)
            {
                TentativeConnection();
            }
            else if (connectionState == STATE_tentative)
            {
                if (NonceRXlocal == rx_buffer[2] &&
                    FHSSgetCurrIndex() == rx_buffer[1])
                {
                    GotConnection();
                }
                else if (2 < (tentative_cnt++))
                {
                    LostConnection();
                }
            }

            FHSSsetCurrIndex(rx_buffer[1]);
            NonceRXlocal = rx_buffer[2];
        }
    }
    else
    {
        rx_buffer_handle = 1;
    }

    // Do freq correction before FHSS
    if ((connectionState > STATE_disconnected) &&
        ((NonceRXlocal + 1) % ExpressLRS_currAirRate->FHSShopInterval) == 0)
    {
        int32_t freqerror = Radio.GetFrequencyError();

#if 0 && !NEW_FREQ_CORR
        freqerror = LPF_FreqError.update(freqerror); // get freq error = 90us
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
                DEBUG_PRINT("FREQ_MIN");
            }
        }

        Radio.setPPMoffsetReg(FreqCorrection);

#else
        //freqerror = LPF_FreqError.update(freqerror);
        //DEBUG_PRINT(freqerror);
        //DEBUG_PRINT(" : ");

        if (abs(freqerror) > 500)
        {
#if 1
            FreqCorrection += freqerror;
            Radio.setPPMoffsetReg(freqerror, 0);
#else
            FreqCorrection += freqerror;
            if (FreqCorrection < FreqCorrectionMin)
                FreqCorrection = FreqCorrectionMin;
            else if (FreqCorrection > FreqCorrectionMax)
                FreqCorrection = FreqCorrectionMax;
#endif
        }
#endif
        //DEBUG_PRINTLN(FreqCorrection - (UID[4] + UID[5]));
    }

#if !FHSS_ONLY_TIMER
    alreadyFHSS = true;
    HandleFHSS();
#endif
    //DEBUG_PRINT("E");
}

void forced_start(void)
{
    DEBUG_PRINTLN("Manual Start");
    Radio.RXnb(GetInitialFreq());
}

void forced_stop(void)
{
    Radio.StopContRX();
    TxTimer.stop();
}

static void SetRFLinkRate(uint8_t rate) // Set speed of RF link (hz)
{
    const expresslrs_mod_settings_s *const config = get_elrs_airRateConfig(rate);
    if (config == ExpressLRS_currAirRate)
        return; // No need to modify, rate is same

    // Stop ongoing actions before configuring next rate
    TxTimer.stop();
    Radio.StopContRX();

    ExpressLRS_currAirRate = config;

    DEBUG_PRINT("Set RF rate: ");
    DEBUG_PRINTLN(config->rate);

    // Init CRC aka LQ array
    LQ_reset();
    // Reset FHSS
    FHSSresetFreqCorrection();
    FHSSsetCurrIndex(0);

    tlm_check_ratio = 0;
    if (TLM_RATIO_NO_TLM < config->TLMinterval)
    {
        tlm_check_ratio = TLMratioEnumToValue(config->TLMinterval) - 1;
    }

    Radio.Config(config->bw, config->sf, config->cr, GetInitialFreq(), 0);
    Radio.SetPreambleLength(config->PreambleLen);
    TxTimer.updateInterval(config->interval);
    LPF_Offset.init(0);
    LPF_FreqError.init(0);
    LPF_UplinkRSSI.init(0);
    crsf.LinkStatistics.uplink_RSSI_2 = 0;
    crsf.LinkStatistics.rf_Mode = RATE_MAX - config->enum_rate;
    Radio.RXnb();
    TxTimer.init();
}

void tx_done_cb(void)
{
    Radio.RXnb(FHSSgetCurrFreq());
}

void setup()
{
    DEBUG_PRINTLN("ExpressLRS RX Module...");
    platform_setup();

    CrsfSerial.Begin(CRSF_RX_BAUDRATE);

    FHSSrandomiseFHSSsequence();

    // Prepare radio
#if defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    Radio.RFmodule = RFMOD_SX1278;
#else
    Radio.RFmodule = RFMOD_SX1276;
#endif
    Radio.SetPins(GPIO_PIN_RST, GPIO_PIN_DIO0, GPIO_PIN_DIO1);
    Radio.SetSyncWord(getSyncWord());
    Radio.Begin();
    Radio.SetOutputPower(0b1111); //default is max power (17dBm for RX)
    Radio.RXdoneCallback1 = &ProcessRFPacketCallback;
    Radio.TXdoneCallback1 = &tx_done_cb;

    // Measure RF noise
#ifdef DEBUG_SERIAL // TODO: Enable this when noize floor is used!
    int16_t RFnoiseFloor = MeasureNoiseFloor();
    DEBUG_PRINT("RF noise floor: ");
    DEBUG_PRINT(RFnoiseFloor);
    DEBUG_PRINTLN("dBm");
    (void)RFnoiseFloor;
#endif

    // Set call back for timer ISR
    TxTimer.callbackTock = &HWtimerCallback;
    // Init first scan index
    scanIndex = RATE_DEFAULT;
    // Initialize CRSF protocol handler
    crsf.Begin();
}

static uint32_t led_toggle_ms = 0;
void loop()
{
    uint32_t now = millis();

    if (rx_buffer_handle)
    {
        process_rx_packet();
        LastValidPacket = now;
        rx_buffer_handle = 0;
        goto exit_loop;
    }

    if (connectionState == STATE_disconnected)
    {
        if (now > RFmodeNextCycle)
        {
            SetRFLinkRate((scanIndex % RATE_MAX)); //switch between rates
            scanIndex++;
            if (RATE_MAX < scanIndex)
                platform_connection_state(STATE_search_iteration_done);

            RFmodeNextCycle = now +
                              ExpressLRS_currAirRate->RFmodeCycleInterval +
                              ExpressLRS_currAirRate->RFmodeCycleAddtionalTime;
        }
        else if (now > led_toggle_ms)
        {
            led_toggle();
            led_toggle_ms = now + 150;
        }
    }
    else if (connectionState > STATE_disconnected)
    {
        // check if we lost conn.
        if (now > (LastValidPacket + ExpressLRS_currAirRate->RFmodeCycleAddtionalTime))
        {
            LostConnection();
        }
        else if (connectionState == STATE_connected)
        {
            if (now > SendLinkStatstoFCintervalNextSend)
            {
                crsf.LinkStatisticsSend();
                SendLinkStatstoFCintervalNextSend = now + SEND_LINK_STATS_TO_FC_INTERVAL;
            }
        }
    }

    crsf.handleUartIn(rx_buffer_handle);

    platform_loop(connectionState);

exit_loop:
    platform_wd_feed();
}

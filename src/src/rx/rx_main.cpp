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
#define RC_DATA_FROM_ISR               1
#define PRINT_FREQ_ERROR               0

///////////////////

CRSF_RX crsf(CrsfSerial); //pass a serial port object to the class for it to use
RcChannels rc_ch;

volatile connectionState_e connectionState = STATE_disconnected;
static volatile uint8_t NonceRXlocal = 0; // nonce that we THINK we are up to.
static volatile uint32_t tlm_check_ratio = 0;
static volatile uint8_t rx_buffer[8] __attribute__((aligned(32)));
static volatile uint8_t rx_buffer_handle = 0;
static volatile uint32_t rx_last_valid_us = 0; //Time the last valid packet was recv
static volatile int32_t rx_freqerror = 0;

///////////////////////////////////////////////
////////////////  Filters  ////////////////////
static LPF LPF_Offset(3);
static LPF LPF_FreqError(4);
static LPF LPF_UplinkRSSI(5);

//////////////////////////////////////////////////////////////
////// Variables for Telemetry and Link Quality //////////////

static volatile uint32_t LastValidPacket = 0; //Time the last valid packet was recv
static uint32_t SendLinkStatstoFCintervalNextSend = SEND_LINK_STATS_TO_FC_INTERVAL;

///////////////////////////////////////////////////////////////
///////////// Variables for Sync Behaviour ////////////////////
static uint32_t RFmodeNextCycle = 0;
static uint32_t RFmodeCycleDelay = 0;
static volatile uint8_t scanIndex = 0;
static uint8_t tentative_cnt = 0;

///////////////////////////////////////

void ICACHE_RAM_ATTR getRFlinkInfo()
{
    int8_t LastRSSI = Radio.LastPacketRSSI;
    //crsf.ChannelsPacked.ch15 = UINT10_to_CRSF(MAP(LastRSSI, -100, -50, 0, 1023));
    //crsf.ChannelsPacked.ch14 = UINT10_to_CRSF(MAP_U16(linkQuality, 0, 100, 0, 1023));
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

uint8_t ICACHE_RAM_ATTR RadioFreqErrorCorr(void)
{
    // Do freq correction before FHSS
    /* Freq tunin ~84us */
    int32_t freqerror = rx_freqerror;
    uint8_t retval = 0;
    rx_freqerror = 0;
    if (!freqerror)
        return 0;

#if PRINT_FREQ_ERROR
    DEBUG_PRINT(" > radio:");
    DEBUG_PRINT(freqerror);
#endif

#define MAX_ERROR 0 //3000
#if MAX_ERROR
    if (freqerror < -MAX_ERROR)
        freqerror = -MAX_ERROR;
    else if (freqerror > MAX_ERROR)
        freqerror = MAX_ERROR;
#endif
    //if (connectionState == STATE_tentative)
    //    LPF_FreqError.init(freqerror);
    freqerror = LPF_FreqError.update(freqerror);

#if PRINT_FREQ_ERROR
    DEBUG_PRINT(" > smooth:");
    DEBUG_PRINT(freqerror);
#endif

    if (abs(freqerror) > 120)
    {
        FreqCorrection += freqerror;
        Radio.setPPMoffsetReg(freqerror, 0);
        retval = 1;
    }
#if PRINT_FREQ_ERROR
    DEBUG_PRINT(" > local:");
    DEBUG_PRINTLN(FreqCorrection - (UID[4] + UID[5]));
#endif

    return retval;
}

uint8_t ICACHE_RAM_ATTR HandleFHSS()
{
    uint8_t fhss = 0;
    if (connectionState > STATE_disconnected) // don't hop if we lost
    {
        //if ((NonceRXlocal & 1) == 0)
        //RadioFreqErrorCorr();
        if ((NonceRXlocal % ExpressLRS_currAirRate->FHSShopInterval) == 0)
        {
            //DEBUG_PRINT("F");
            linkQuality = LQ_getlinkQuality();
            Radio.NonceRX = 0;
            FHSSincCurrIndex();
            fhss = 1;
        }
        NonceRXlocal++;
    }
    return fhss;
}

void ICACHE_RAM_ATTR HandleSendTelemetryResponse() // total ~79us
{
    //DEBUG_PRINT("X");
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
    uint8_t fhss_config_rx = 0;

#if PRINT_TIMER
    DEBUG_PRINT(" us ");
    DEBUG_PRINT(us);
#endif
    /* do adjustment */
    if (rx_last_valid_us)
    {
        int32_t diff_us = (int32_t)(us - rx_last_valid_us);
#if PRINT_TIMER
        DEBUG_PRINT(" - rx ");
        DEBUG_PRINT(rx_last_valid_us);
        DEBUG_PRINT(" = ");
        DEBUG_PRINT(diff_us);
#endif
        rx_last_valid_us = 0;
        /* Adjust timer */
        if (abs(diff_us) > 100)
            TxTimer.reset(diff_us - TIMER_OFFSET);
    }
    else
    {
        TxTimer.setTime(); // Reset timer interval
    }

    fhss_config_rx |= HandleFHSS();
    if (fhss_config_rx || connectionState == STATE_connected)
        fhss_config_rx |= RadioFreqErrorCorr();
#if PRINT_TIMER
    DEBUG_PRINT(" nonce ");
    DEBUG_PRINT(NonceRXlocal);
#endif

    // Check if connected before sending tlm resp
    if (STATE_disconnected < connectionState)
    {
        LQ_nextPacket();
        if (tlm_check_ratio && (NonceRXlocal & tlm_check_ratio) == 0)
        {
            HandleSendTelemetryResponse();
            fhss_config_rx = 0;
        }
    }
    if (fhss_config_rx)
        Radio.RXnb(FHSSgetCurrFreq()); // 260us => 148us => ~67us

#if PRINT_TIMER
    DEBUG_PRINTLN("");
#endif
}

void ICACHE_RAM_ATTR LostConnection()
{
    if (connectionState <= STATE_disconnected)
    {
        return; // Already disconnected
    }

    TxTimer.stop(); // Stop sync timer

    connectionState = STATE_disconnected; //set lost connection
    scanIndex = RATE_DEFAULT;

    led_set_state(1);                     // turn off led
    Radio.SetFrequency(GetInitialFreq()); // in conn lost state we always want to listen on freq index 0
    DEBUG_PRINTLN("lost conn");

    platform_connection_state(connectionState);
}

void ICACHE_RAM_ATTR TentativeConnection()
{
    TxTimer.start(); // Start local sync timer

    /* Do initial freq correction */
    FreqCorrection += rx_freqerror;
    Radio.setPPMoffsetReg(rx_freqerror, 0);
    rx_freqerror = 0;

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

#if !RC_DATA_FROM_ISR
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
#endif // !RC_DATA_FROM_ISR

void ICACHE_RAM_ATTR ProcessRFPacketCallback(uint8_t *buff)
{
    //DEBUG_PRINT("I");
    uint32_t current_us = Radio.LastPacketIsrMicros; //micros();

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

    rx_freqerror = Radio.GetFrequencyError();

    rx_last_valid_us = current_us;

    //LastValidPacket = current_us / 1000; //us gives 1.18 hours, millis();
    LastValidPacket = millis();

    switch (TYPE_GET(address))
    {
        case SYNC_PACKET:
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
                //TxTimer.reset();
            }
            break;
        }
#if RC_DATA_FROM_ISR
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
#endif // RC_DATA_FROM_ISR

        default:
#if !RC_DATA_FROM_ISR
            rx_buffer_handle = 1;
#endif // !RC_DATA_FROM_ISR
            break;
    }

    LQ_setPacketState(1);
    getRFlinkInfo();

    //DEBUG_PRINT("E");
}

void forced_start(void)
{
    DEBUG_PRINTLN("Manual Start");
    Radio.NonceRX = 0;
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

    RFmodeCycleDelay = ExpressLRS_currAirRate->RFmodeCycleInterval +
                       ExpressLRS_currAirRate->RFmodeCycleAddtionalTime;

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
    crsf.LinkStatistics.rf_Mode = RATE_GET_OSD_NUM(config->enum_rate); //RATE_MAX - config->enum_rate;
    Radio.NonceRX = 0;
    Radio.RXnb();
    TxTimer.init();
}

void tx_done_cb(void)
{
    Radio.NonceRX = 0;
    Radio.RXnb(FHSSgetCurrFreq()); // ~67us
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

#if !RC_DATA_FROM_ISR
    if (rx_buffer_handle)
    {
        process_rx_packet();
        LastValidPacket = now;
        rx_buffer_handle = 0;
        goto exit_loop;
    }
#endif // !RC_DATA_FROM_ISR

    if (connectionState == STATE_disconnected)
    {
        if (RFmodeCycleDelay < (now - RFmodeNextCycle))
        {
            SetRFLinkRate((scanIndex % RATE_MAX)); //switch between rates
            scanIndex++;
            if (RATE_MAX < scanIndex)
                platform_connection_state(STATE_search_iteration_done);

            RFmodeNextCycle = now;
        }
        else if (150 < (now - led_toggle_ms))
        {
            led_toggle();
            led_toggle_ms = now;
        }
    }
    else if (connectionState > STATE_disconnected)
    {
        // check if we lost conn.
        if (ExpressLRS_currAirRate->RFmodeCycleAddtionalTime < (int32_t)(now - LastValidPacket))
        {
            LostConnection();
        }
        else if (connectionState == STATE_connected)
        {
            if (SEND_LINK_STATS_TO_FC_INTERVAL <= (now - SendLinkStatstoFCintervalNextSend))
            {
                crsf.LinkStatisticsSend();
                SendLinkStatstoFCintervalNextSend = now;
            }
        }
    }

    crsf.handleUartIn(rx_buffer_handle);

    platform_loop(connectionState);

#if !RC_DATA_FROM_ISR
exit_loop:
#endif
    platform_wd_feed();
}

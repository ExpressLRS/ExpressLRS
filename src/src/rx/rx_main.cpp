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
#define PRINT_FREQ_ERROR               0

///////////////////

CRSF_RX crsf(CrsfSerial); //pass a serial port object to the class for it to use
RcChannels rc_ch;

volatile connectionState_e connectionState = STATE_disconnected;
static volatile uint8_t NonceRXlocal = 0; // nonce that we THINK we are up to.
static volatile uint32_t tlm_check_ratio = 0;
static volatile uint32_t rx_last_valid_us = 0; //Time the last valid packet was recv
static volatile int32_t rx_freqerror = 0;

///////////////////////////////////////////////
////////////////  Filters  ////////////////////
static LPF LPF_FreqError(4);
static LPF LPF_UplinkRSSI(5);

//////////////////////////////////////////////////////////////
////// Variables for Telemetry and Link Quality //////////////

static volatile uint32_t LastValidPacket = 0; //Time the last valid packet was recv
static uint32_t SendLinkStatstoFCintervalNextSend = SEND_LINK_STATS_TO_FC_INTERVAL;
static mspPacket_t msp_packet_rx;
static mspPacket_t msp_packet_tx;
static volatile uint_fast8_t tlm_msp_send = 0;

///////////////////////////////////////////////////////////////
///////////// Variables for Sync Behaviour ////////////////////
static uint32_t RFmodeNextCycle = 0;
static uint32_t RFmodeCycleDelay = 0;
static volatile uint8_t scanIndex = 0;
static uint8_t tentative_cnt = 0;

///////////////////////////////////////

static bool ledState = false;
inline void led_set_state(bool state)
{
    ledState = state;
    platform_set_led(state);
}

inline void led_toggle(void)
{
    led_set_state(!ledState);
}

static void ICACHE_RAM_ATTR handle_tlm_ratio(uint8_t TLMinterval)
{
    if ((TLM_RATIO_NO_TLM < TLMinterval) && (TLM_RATIO_MAX > TLMinterval))
    {
        tlm_check_ratio = TLMratioEnumToValue(TLMinterval) - 1;
    }
    else
    {
        tlm_check_ratio = 0;
    }
}
///////////////////////////////////////

void ICACHE_RAM_ATTR getRFlinkInfo()
{
    int8_t LastRSSI = Radio.LastPacketRSSI;
    //crsf.ChannelsPacked.ch15 = UINT10_to_CRSF(MAP(LastRSSI, -100, -50, 0, 1023));
    //crsf.ChannelsPacked.ch14 = UINT10_to_CRSF(MAP_U16(linkQuality, 0, 100, 0, 1023));
    int32_t rssiDBM = LPF_UplinkRSSI.update(LastRSSI);
    // our rssiDBM is currently in the range -128 to 98, but BF wants a value in the range
    // 0 to 255 that maps to -1 * the negative part of the rssiDBM, so cap at 0.
    if (rssiDBM > 0)
        rssiDBM = 0;
    crsf.LinkStatistics.uplink_RSSI_1 = -1 * rssiDBM; // to match BF
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
    DEBUG_PRINT(FreqCorrection - (UID[4] + UID[5]));
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

    if (tlm_msp_send)
    {
        /* send msp packet if needed */
        tx_buffer[0] = DEIVCE_ADDR_GENERATE(DeviceAddr) + DL_PACKET_TLM_MSP;

        if (rc_ch.tlm_send(tx_buffer, msp_packet_tx))
        {
            msp_packet_tx.reset();
            tlm_msp_send = 0;
        }
    }
    else
    {
        tx_buffer[0] = DEIVCE_ADDR_GENERATE(DeviceAddr) + DL_PACKET_TLM_LINK; // address + tlm packet
        crsf.LinkStatisticsPack(&tx_buffer[1]);
    }
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
    DEBUG_PRINT("us ");
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
#if 0
        // allow max 500us correction
        if (diff_us < -500)
            diff_us = -500;
        else if (diff_us > 500)
            diff_us = 500;
#else
        int32_t interval = ExpressLRS_currAirRate->interval;
        if (diff_us > (interval >> 1))
        {
            /* the timer was called too early */
            diff_us %= interval;
            diff_us -= interval;
        }
#endif

        /* Adjust the timer */
        if ((TIMER_OFFSET_LIMIT < diff_us) || (diff_us < 0))
            TxTimer.reset(diff_us - TIMER_OFFSET);
        //else if ((diff_us < 0))
        //    TxTimer.reset(diff_us);
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

#if PRINT_TIMER || PRINT_FREQ_ERROR
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
    TxTimer.start();                     // Start local sync timer
    TxTimer.setTime(TIMER_OFFSET_LIMIT); // Trigger isr right after reception
    /* Do initial freq correction */
    FreqCorrection += rx_freqerror;
    Radio.setPPMoffsetReg(rx_freqerror, 0);
    rx_freqerror = 0;
    rx_last_valid_us = 0;

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

void ICACHE_RAM_ATTR ProcessRFPacketCallback(uint8_t *rx_buffer)
{
    //DEBUG_PRINT("I");
    const connectionState_e _conn_state = connectionState;
    const uint32_t current_us = Radio.LastPacketIsrMicros;
    const uint8_t calculatedCRC = CalcCRC(rx_buffer, 7) + CRCCaesarCipher;
    ElrsSyncPacket_s const * const sync = (ElrsSyncPacket_s*)rx_buffer;
    const uint8_t address = sync->address;

    if (sync->crc != calculatedCRC)
    {
        DEBUG_PRINT("!C");
        return;
    }
    else if (DEIVCE_ADDR_GET(address) != DeviceAddr)
    {
        DEBUG_PRINT("!A");
        return;
    }

    rx_freqerror = Radio.GetFrequencyError();

    rx_last_valid_us = current_us;
    LastValidPacket = millis();

    switch (TYPE_GET(address))
    {
        case UL_PACKET_SYNC:
        {
            if (sync->uid3 == UID[3] &&
                sync->uid4 == UID[4] &&
                sync->uid5 == UID[5])
            {
                if (_conn_state == STATE_disconnected)
                {
                    TentativeConnection();
                }
                else if (_conn_state == STATE_tentative)
                {
                    if (NonceRXlocal == sync->rxtx_counter &&
                        FHSSgetCurrIndex() == sync->fhssIndex)
                    {
                        GotConnection();
                    }
                    else if (2 < (tentative_cnt++))
                    {
                        LostConnection();
                    }
                }
                else
                {
                    // Send last command to FC if connected to keep it running
                    crsf.sendRCFrameToFC();
                }

#if 0
                if (ExpressLRS_currAirRate->enum_rate != sync->air_rate)
                    SetRFLinkRate(sync->air_rate);
#endif
                handle_tlm_ratio(sync->tlm_interval);
                FHSSsetCurrIndex(sync->fhssIndex);
                NonceRXlocal = sync->rxtx_counter;
            }
            break;
        }
        case UL_PACKET_RC_DATA: //Standard RC Data Packet
            if (_conn_state == STATE_connected)
            {
                rc_ch.channels_extract(rx_buffer, crsf.ChannelsPacked);
                crsf.sendRCFrameToFC();
            }
            break;

#if !defined(SEQ_SWITCHES) && !defined(HYBRID_SWITCHES_8)
        case UL_PACKET_SWITCH_DATA: // Switch Data Packet
            // extra layer of protection incase the crc and addr headers fail us.
            if ((rx_buffer[3] == rx_buffer[1]) &&
                (rx_buffer[4] == rx_buffer[2]))
            {
                if (_conn_state == STATE_connected)
                {
                    rc_ch.channels_extract(rx_buffer, crsf.ChannelsPacked);
                    crsf.sendRCFrameToFC();
                }
                NonceRXlocal = rx_buffer[5];
                FHSSsetCurrIndex(rx_buffer[6]);
            }
            break;
#endif

        case UL_PACKET_MSP:
            if (rc_ch.tlm_receive(rx_buffer, msp_packet_rx))
            {
                // TODO: Check if packet is for receiver
                crsf.sendMSPFrameToFC(msp_packet_rx);
                msp_packet_rx.reset();
            }
            break;

        default:
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

    handle_tlm_ratio(config->TLMinterval);

    Radio.Config(config->bw, config->sf, config->cr, GetInitialFreq(), 0);
    Radio.SetPreambleLength(config->PreambleLen);
    TxTimer.updateInterval(config->interval);
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

/* FC sends v1 MSPs */
static void msp_data_cb(uint8_t const *const input)
{
    if (tlm_msp_send)
        return;

    /* process MSP packet from flight controller
     *  [0] header: seq&0xF,
     *  [1] payload size
     *  [2] function
     *  [3...] payload + crc
     */
    mspHeaderV1_t *hdr = (mspHeaderV1_t *)input;
    msp_packet_tx.reset(hdr);
    msp_packet_tx.type = MSP_PACKET_V1_CMD;
    if (hdr->payloadSize)
        volatile_memcpy(msp_packet_tx.payload,
                        hdr->payload,
                        hdr->payloadSize);

    tlm_msp_send = 1; // rdy for sending
}

void setup()
{
    msp_packet_rx.reset();
    msp_packet_tx.reset();

    DEBUG_PRINTLN("ExpressLRS RX Module...");
    platform_setup();

    CrsfSerial.Begin(CRSF_RX_BAUDRATE);

    //FHSSrandomiseFHSSsequence();

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
    int RFnoiseFloor = Radio.MeasureNoiseFloor(10, GetInitialFreq());
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
    crsf.MspCallback = msp_data_cb;
    crsf.Begin();
}

static uint32_t led_toggle_ms = 0;
void loop()
{
    uint32_t now = millis();
    uint8_t rx_buffer_handle = 0;

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

    platform_wd_feed();
}

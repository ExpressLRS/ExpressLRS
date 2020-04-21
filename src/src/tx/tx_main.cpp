#include <Arduino.h>
#include "FIFO.h"
#include "utils.h"
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF_TX.h"
#include "FHSS.h"
#include "targets.h"
#include "POWERMGNT.h"
#include "HwTimer.h"
#include "debug.h"
#include "rc_channels.h"

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT        1500U // After 1500ms of no TLM response consider that slave has lost connection
#define LQ_CALCULATE_INTERVAL             500u
#define SYNC_PACKET_SEND_INTERVAL_RX_LOST 250u  // how often to send the switch data packet (ms) when there is no response from RX
#define SYNC_PACKET_SEND_INTERVAL_RX_CONN 1500u // how often to send the switch data packet (ms) when there we have a connection
///////////////////

/// define some libs to use ///
CRSF_TX crsf(CrsfSerial);
RcChannels rc_ch;
POWERMGNT PowerMgmt;
static volatile uint32_t _rf_rxtx_counter = 0;

/////////// SYNC PACKET ////////
static uint32_t SyncPacketNextSend = 0;

/////////// CONNECTION /////////
static volatile uint32_t LastPacketRecvMillis = 0;
volatile connectionState_e connectionState = STATE_disconnected;

//////////// TELEMETRY /////////
static volatile uint32_t recv_tlm_counter = 0;
static volatile uint8_t downlink_linkQuality = 0;
static uint32_t PacketRateNextCheck = 0;

//////////// LUA /////////

///////////////////////////////////////

static void ICACHE_RAM_ATTR ProcessTLMpacket()
{
    uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
    uint8_t in_byte = Radio.RXdataBuffer[7];

    //DEBUG_PRINTLN("TLMpacket0");

    if (in_byte != calculatedCRC)
    {
        DEBUG_PRINTLN("TLM crc error");
        return;
    }

    in_byte = Radio.RXdataBuffer[0];

    if (DEIVCE_ADDR_GET(in_byte) != DeviceAddr)
    {
        DEBUG_PRINTLN("TLM device address error");
        return;
    }

    connectionState = STATE_connected;
    platform_connection_state(STATE_connected);
    LastPacketRecvMillis = (millis() + RX_CONNECTION_LOST_TIMEOUT);

    if (TYPE_GET(in_byte) == TLM_PACKET)
    {
        recv_tlm_counter++;
        crsf.LinkStatisticsExtract(&Radio.RXdataBuffer[1],
                                   Radio.LastPacketSNR,
                                   Radio.LastPacketRSSI,
                                   downlink_linkQuality);
    }
}

///////////////////////////////////////

static void SetRFLinkRate(uint8_t rate, uint8_t init = 0) // Set speed of RF link (hz)
{
    // TODO: Protect this by disabling timer/isr...

    const expresslrs_mod_settings_s *const config = get_elrs_airRateConfig(rate);
    if (config == ExpressLRS_currAirRate)
        return; // No need to modify, rate is same

    if (!init)
    {
        // Stop timer and put radio into sleep
        TxTimer.stop();
        Radio.StopContRX();
    }

    ExpressLRS_currAirRate = config;
    TxTimer.updateInterval(config->interval); // TODO: Make sure this is equiv to above commented lines

    Radio.Config(config->bw, config->sf, config->cr, GetInitialFreq(), 0);
    Radio.SetPreambleLength(config->PreambleLen);
    crsf.setRcPacketRate(config->interval);
    crsf.LinkStatistics.rf_Mode = RATE_MAX - ExpressLRS_currAirRate->enum_rate; // 4 ??

    // Set connected if telemetry is not used
    connectionState = (TLM_RATIO_NO_TLM == config->TLMinterval) ? STATE_connected : STATE_disconnected;
    platform_connection_state(connectionState);

    if (!init)
        TxTimer.start();
}

static void ICACHE_RAM_ATTR HandleFHSS_TX()
{
    // Called after successful TX
    uint8_t modresult = (_rf_rxtx_counter) % ExpressLRS_currAirRate->FHSShopInterval;
    if (modresult == 0) // if it time to hop, do so.
    {
        FHSSincCurrIndex();
    }
    _rf_rxtx_counter++;
}

static void ICACHE_RAM_ATTR HandleTLM()
{
    // Called after successful TX
    if (ExpressLRS_currAirRate->TLMinterval > TLM_RATIO_NO_TLM)
    {
        uint8_t modresult = (_rf_rxtx_counter) % TLMratioEnumToValue(ExpressLRS_currAirRate->TLMinterval);
        if (modresult == 0)
        {
            PowerMgmt.pa_off();
            Radio.RXnb(FHSSgetCurrFreq());
        }
    }
}

static void ICACHE_RAM_ATTR GenerateSyncPacketData(uint8_t *const output)
{
    output[0] = (DeviceAddr) + SYNC_PACKET;
    output[1] = FHSSgetCurrIndex();
    output[2] = _rf_rxtx_counter;
    output[3] = 0;
    output[4] = UID[3];
    output[5] = UID[4];
    output[6] = UID[5];
}

static void ICACHE_RAM_ATTR SendRCdataToRF()
{
    // Called by HW timer
    uint32_t current_ms;
    uint32_t __tx_buffer[2]; // esp requires aligned buffer
    uint8_t *tx_buffer = (uint8_t *)__tx_buffer;

    //DEBUG_PRINT("I");

    crsf.UpdateOpenTxSyncOffset(); // tells the crsf that we want to send data now - this allows opentx packet syncing

    /////// This Part Handles the Telemetry RX ///////
    if (ExpressLRS_currAirRate->TLMinterval > TLM_RATIO_NO_TLM)
    {
        uint8_t modresult = (_rf_rxtx_counter) % TLMratioEnumToValue(ExpressLRS_currAirRate->TLMinterval);
        if (modresult == 0)
        {
            // Skip TX because TLM RX is ongoing and increate TX cnt to keep sync
            goto exit_rx_send;
        }
    }

    current_ms = millis();

    //only send sync when its time and only on channel 0;
    if ((current_ms > SyncPacketNextSend) && (Radio.currFreq == GetInitialFreq()))
    {
        GenerateSyncPacketData(tx_buffer);
        SyncPacketNextSend =
            current_ms +
            ((connectionState != STATE_connected) ? SYNC_PACKET_SEND_INTERVAL_RX_LOST : SYNC_PACKET_SEND_INTERVAL_RX_CONN);
    }
    else
    {
        rc_ch.channels_pack(tx_buffer);
    }

    // Calculate the CRC
    tx_buffer[7] = CalcCRC(tx_buffer, 7) + CRCCaesarCipher;
    // Enable PA
    PowerMgmt.pa_on();
    // Send data to rf
    Radio.TXnb(tx_buffer, 8, FHSSgetCurrFreq());
exit_rx_send:
    // Increase TX counter
    HandleFHSS_TX();
}

static void decRFLinkRate(void)
{
    if (ExpressLRS_currAirRate->enum_rate < (RATE_MAX - 1))
    {
        SetRFLinkRate((ExpressLRS_currAirRate->enum_rate + 1));
    }
}

static void incRFLinkRate(void)
{
    if (ExpressLRS_currAirRate->enum_rate > RATE_200HZ)
    {
        SetRFLinkRate((ExpressLRS_currAirRate->enum_rate - 1));
    }
}

static void ParamUpdateReq(uint8_t const *msg, uint16_t len)
{
    // Called from UART handling loop (main loop)

    if (len != 2)
        return;

    uint8_t value = msg[1];

    switch (msg[0])
    {
        case 0: // send all params
            break;

        case 1:
            if (value == 0)
            {
                decRFLinkRate();
            }
            else if (value == 1)
            {
                incRFLinkRate();
            }
            DEBUG_PRINT("Rate: ");
            DEBUG_PRINTLN(ExpressLRS_currAirRate->enum_rate);
            break;

        case 2:
            break;

        case 3:
            if (value == 0)
            {
                PowerMgmt.decPower();
            }
            else if (value == 1)
            {
                PowerMgmt.incPower();
            }
            crsf.LinkStatistics.downlink_TX_Power = PowerMgmt.power_to_radio_enum();
            DEBUG_PRINT("Power: ");
            DEBUG_PRINTLN(PowerMgmt.currPower());
            break;

        case 4:
            break;

        default:
            break;
    }

    crsf.sendLUAresponseToRadio((ExpressLRS_currAirRate->enum_rate + 2),
                                ExpressLRS_currAirRate->TLMinterval + 1,
                                PowerMgmt.currPower() + 2,
                                4);
}

static void hw_timer_init(void)
{
    TxTimer.init();
}
static void hw_timer_stop(void)
{
    TxTimer.stop();
}

static void rc_data_cb(crsf_channels_t const *const channels)
{
    rc_ch.processChannels(channels);
}

void setup()
{
    CrsfSerial.Begin(CRSF_OPENTX_BAUDRATE);

    platform_setup();

    crsf.connected = hw_timer_init; // it will auto init when it detects UART connection
    crsf.disconnected = hw_timer_stop;
    crsf.RecvParameterUpdate = ParamUpdateReq;
    crsf.RCdataCallback1 = rc_data_cb;

    TxTimer.callbackTock = &SendRCdataToRF;

    Radio.SetPins(GPIO_PIN_RST, GPIO_PIN_DIO0, GPIO_PIN_DIO1);
    Radio.SetSyncWord(getSyncWord());
    Radio.Begin(GPIO_PIN_TX_ENABLE, GPIO_PIN_RX_ENABLE);

    DEBUG_PRINTLN("ExpressLRS TX Module Booted...");

    FHSSrandomiseFHSSsequence();

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915)
#ifdef Regulatory_Domain_EU_868
    DEBUG_PRINTLN("Setting 868MHz Mode");
#else
    DEBUG_PRINTLN("Setting 915MHz Mode");
#endif
    Radio.RFmodule = RFMOD_SX1276; //define radio module here

#elif defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    DEBUG_PRINTLN("Setting 433MHz Mode");
    Radio.RFmodule = RFMOD_SX1278; //define radio module here
#endif

    Radio.RXdoneCallback1 = &ProcessTLMpacket;

    //Radio.TXdoneCallback1 = &HandleFHSS_TX;
    Radio.TXdoneCallback2 = &HandleTLM;
    //Radio.TXdoneCallback3 = ;
    //Radio.TXdoneCallback4 = ;

    PowerMgmt.defaultPower();
    crsf.LinkStatistics.downlink_TX_Power = PowerMgmt.power_to_radio_enum();

    SetRFLinkRate(RATE_DEFAULT, 1);

    crsf.Begin();
}

void loop()
{
    if (TLM_RATIO_NO_TLM < ExpressLRS_currAirRate->TLMinterval)
    {
        uint32_t current_ms = millis();

        if (connectionState > STATE_disconnected &&
            current_ms > LastPacketRecvMillis)
        {
            connectionState = STATE_disconnected;
            platform_connection_state(STATE_disconnected);
        }
        else if (current_ms >= PacketRateNextCheck)
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
    crsf.handleUartIn();

    platform_loop(connectionState);
    platform_wd_feed();
}

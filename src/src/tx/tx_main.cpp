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

static void SetRFLinkRate(uint8_t rate, uint8_t init = 0);

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
static volatile uint8_t rx_buffer[8];
static volatile uint8_t rx_buffer_handle = 0;

/////////// SYNC PACKET ////////
static uint32_t SyncPacketNextSend = 0;

/////////// CONNECTION /////////
static uint32_t LastPacketRecvMillis = 0;
volatile connectionState_e connectionState = STATE_disconnected;

//////////// TELEMETRY /////////
static uint32_t recv_tlm_counter = 0;
static uint8_t downlink_linkQuality = 0;
static uint32_t PacketRateNextCheck = 0;
static volatile uint32_t tlm_check_ratio = 0;

//////////// LUA /////////

///////////////////////////////////////

static void process_rx_buffer()
{
    uint8_t calculatedCRC = CalcCRC(rx_buffer, 7) + CRCCaesarCipher;
    uint8_t in_byte = rx_buffer[0];

    if (rx_buffer[7] != calculatedCRC)
    {
        DEBUG_PRINT("!C");
        return;
    }
    else if (DEIVCE_ADDR_GET(in_byte) != DeviceAddr)
    {
        DEBUG_PRINT("!A");
        return;
    }

    connectionState = STATE_connected;
    platform_connection_state(STATE_connected);
    LastPacketRecvMillis = millis();

    if (TYPE_GET(in_byte) == TLM_PACKET)
    {
        recv_tlm_counter++;
        crsf.LinkStatisticsExtract(&rx_buffer[1],
                                   Radio.LastPacketSNR,
                                   Radio.LastPacketRSSI,
                                   downlink_linkQuality);
    }
}

static void ICACHE_RAM_ATTR ProcessTLMpacket(uint8_t *buff)
{
    volatile_memcpy(rx_buffer, buff, sizeof(rx_buffer));
    rx_buffer_handle = 1;

    //DEBUG_PRINT("R");
}

static void ICACHE_RAM_ATTR HandleTLM()
{
    if (tlm_check_ratio && (_rf_rxtx_counter & tlm_check_ratio) == 0)
    {
        // receive tlm package
        PowerMgmt.pa_off();
        Radio.RXnb(FHSSgetCurrFreq());
    }
}

///////////////////////////////////////

static void ICACHE_RAM_ATTR HandleFHSS_TX()
{
    // Called from HW ISR context
    if ((_rf_rxtx_counter % ExpressLRS_currAirRate->FHSShopInterval) == 0)
    {
        // it is time to hop
        FHSSincCurrIndex();
    }
    _rf_rxtx_counter++;
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

static void ICACHE_RAM_ATTR SendRCdataToRF(uint32_t current_us)
{
    // Called by HW timer
    uint32_t current_ms = current_us / 1000U;
    uint32_t sync_send_interval = ((connectionState != STATE_connected) ? SYNC_PACKET_SEND_INTERVAL_RX_LOST : SYNC_PACKET_SEND_INTERVAL_RX_CONN);
    uint32_t freq;
    uint32_t __tx_buffer[2]; // esp requires aligned buffer
    uint8_t *tx_buffer = (uint8_t *)__tx_buffer;

    //DEBUG_PRINT("I");

    crsf.UpdateOpenTxSyncOffset(current_us); // tells the crsf that we want to send data now - this allows opentx packet syncing

    // Check if telemetry RX ongoing
    if (tlm_check_ratio && (_rf_rxtx_counter & tlm_check_ratio) == 0)
    {
        // Skip TX because TLM RX is ongoing
        goto exit_rx_send;
    }

    freq = FHSSgetCurrFreq();

    //only send sync when its time and only on channel 0;
    if ((freq == GetInitialFreq()) && (sync_send_interval <= (current_ms - SyncPacketNextSend)))
    {
        GenerateSyncPacketData(tx_buffer);
        SyncPacketNextSend = current_ms;
    }
    else
    {
        rc_ch.get_packed_data(tx_buffer);
    }

    // Calculate the CRC
    tx_buffer[7] = CalcCRC(tx_buffer, 7) + CRCCaesarCipher;
    // Enable PA
    PowerMgmt.pa_on();
    // Send data to rf
    Radio.TXnb(tx_buffer, 8, freq);
exit_rx_send:
    // Increase TX counter
    HandleFHSS_TX();
}

///////////////////////////////////////

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

static void ParamWriteHandler(uint8_t const *msg, uint16_t len)
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

    uint8_t resp[4] = {(uint8_t)(ExpressLRS_currAirRate->enum_rate + 2),
                       (uint8_t)(ExpressLRS_currAirRate->TLMinterval + 1),
                       (uint8_t)(PowerMgmt.currPower() + 2),
                       4u};
    crsf.sendLUAresponseToRadio(resp, sizeof(resp));
}

///////////////////////////////////////

static void SetRFLinkRate(uint8_t rate, uint8_t init) // Set speed of RF link (hz)
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
    if (TLM_RATIO_NO_TLM == config->TLMinterval)
    {
        Radio.RXdoneCallback1 = SX127xDriver::rx_nullCallback;
        Radio.TXdoneCallback1 = SX127xDriver::tx_nullCallback;
        // Set connected if telemetry is not used
        connectionState = STATE_connected;
        tlm_check_ratio = 0;
    }
    else
    {
        Radio.RXdoneCallback1 = ProcessTLMpacket;
        Radio.TXdoneCallback1 = HandleTLM;
        connectionState = STATE_disconnected;
        tlm_check_ratio = TLMratioEnumToValue(ExpressLRS_currAirRate->TLMinterval) - 1;
    }
    platform_connection_state(connectionState);

    if (!init)
        TxTimer.start();
}

static void hw_timer_init(void)
{
    TxTimer.init();
    TxTimer.start();
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
    DEBUG_PRINTLN("ExpressLRS TX Module...");
    CrsfSerial.Begin(CRSF_TX_BAUDRATE_FAST);

    platform_setup();

    crsf.connected = hw_timer_init; // it will auto init when it detects UART connection
    crsf.disconnected = hw_timer_stop;
    crsf.ParamWriteCallback = ParamWriteHandler;
    crsf.RCdataCallback1 = rc_data_cb;

    TxTimer.callbackTock = &SendRCdataToRF;

    FHSSrandomiseFHSSsequence();

#if defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    Radio.RFmodule = RFMOD_SX1278;
#else
    Radio.RFmodule = RFMOD_SX1276;
#endif
    Radio.SetPins(GPIO_PIN_RST, GPIO_PIN_DIO0, GPIO_PIN_DIO1);
    Radio.SetSyncWord(getSyncWord());
    Radio.Begin(GPIO_PIN_TX_ENABLE, GPIO_PIN_RX_ENABLE);

    PowerMgmt.defaultPower();
    crsf.LinkStatistics.downlink_TX_Power = PowerMgmt.power_to_radio_enum();

    SetRFLinkRate(current_rate_config, 1);

    crsf.Begin();
}

void loop()
{
    if (rx_buffer_handle)
    {
        process_rx_buffer();
        rx_buffer_handle = 0;
        goto exit_loop;
    }

    if (TLM_RATIO_NO_TLM < ExpressLRS_currAirRate->TLMinterval)
    {
        uint32_t current_ms = millis();

        if (connectionState > STATE_disconnected &&
            RX_CONNECTION_LOST_TIMEOUT < (current_ms - LastPacketRecvMillis))
        {
            connectionState = STATE_disconnected;
            platform_connection_state(STATE_disconnected);
        }
        else if (LQ_CALCULATE_INTERVAL <= (current_ms - PacketRateNextCheck))
        {
            PacketRateNextCheck = current_ms;

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
    crsf.handleUartIn(rx_buffer_handle);

    platform_loop(connectionState);

exit_loop:
    platform_wd_feed();
}

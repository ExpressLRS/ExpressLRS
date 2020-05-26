#include <Arduino.h>
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
#include <stdlib.h>

static uint8_t SetRFLinkRate(uint8_t rate, uint8_t init = 0);

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT        1500U // After 1500ms of no TLM response consider that slave has lost connection
#define LQ_CALCULATE_INTERVAL             500u
#ifndef TLM_REPORT_INTERVAL
#define TLM_REPORT_INTERVAL               300u
#endif
//#define SYNC_PACKET_SEND_INTERVAL_RX_LOST 150000u //  250000u
//#define SYNC_PACKET_SEND_INTERVAL_RX_CONN 350000u // 1500000u
#define SYNC_PACKET_SEND_INTERVAL_RX_LOST 250000u
#define SYNC_PACKET_SEND_INTERVAL_RX_CONN SYNC_PACKET_SEND_INTERVAL_RX_LOST

///////////////////

/// define some libs to use ///
SX127xDriver Radio(RadioSpi);
CRSF_TX crsf(CrsfSerial);
RcChannels rc_ch;
POWERMGNT PowerMgmt(Radio);
/*static*/ volatile uint32_t DRAM_ATTR _rf_rxtx_counter = 0;
static volatile uint8_t rx_buffer[8];
static volatile uint8_t rx_buffer_handle = 0;
static volatile uint8_t red_led_state = 0;

struct platform_config pl_config = {
    .key = 0,
    .mode = RATE_DEFAULT,
    .power = TX_POWER_DEFAULT,
    .tlm = TLM_RATIO_DEFAULT,
};

/////////// SYNC PACKET ////////
static uint32_t DRAM_ATTR SyncPacketNextSend = 0;
static volatile uint32_t DRAM_ATTR sync_send_interval = SYNC_PACKET_SEND_INTERVAL_RX_LOST;

/////////// CONNECTION /////////
static uint32_t LastPacketRecvMillis = 0;
volatile connectionState_e DRAM_ATTR connectionState = STATE_disconnected;

//////////// TELEMETRY /////////
static volatile uint32_t expected_tlm_counter = 0;
static uint32_t recv_tlm_counter = 0;
static uint8_t downlink_linkQuality = 0;
static uint32_t PacketRateNextCheck = 0;
static volatile uint32_t DRAM_ATTR tlm_check_ratio = 0;
static volatile uint_fast8_t DRAM_ATTR TLMinterval = 0;
static mspPacket_t msp_packet_tx;
static mspPacket_t msp_packet_rx;
static volatile uint_fast8_t tlm_send = 0;
static uint32_t TlmSentToRadioTime = 0;

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
    //sync_send_interval = SYNC_PACKET_SEND_INTERVAL_RX_CONN;
    platform_connection_state(STATE_connected);
    platform_set_led(0);
    LastPacketRecvMillis = millis();
    recv_tlm_counter++;

    switch (TYPE_GET(in_byte))
    {
        case DL_PACKET_TLM_MSP:
        {
            rc_ch.tlm_receive(rx_buffer, msp_packet_rx);
            break;
        }
        case DL_PACKET_TLM_LINK:
        {
            crsf.LinkStatisticsExtract(&rx_buffer[1],
                                       Radio.LastPacketSNR,
                                       Radio.LastPacketRSSI);
            break;
        }
        case DL_PACKET_FREE1:
        case DL_PACKET_FREE2:
        default:
            break;
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
        expected_tlm_counter++;
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
    ElrsSyncPacket_s * sync = (ElrsSyncPacket_s*)output;
    sync->address = (DeviceAddr) + UL_PACKET_SYNC;
    sync->fhssIndex = FHSSgetCurrIndex();
    sync->rxtx_counter = _rf_rxtx_counter;
    sync->air_rate = ExpressLRS_currAirRate->enum_rate;
    sync->tlm_interval = TLMinterval;
    sync->uid3 = UID[3];
    sync->uid4 = UID[4];
    sync->uid5 = UID[5];
}

static void ICACHE_RAM_ATTR SendRCdataToRF(uint32_t current_us)
{
    // Called by HW timer
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
    if ((FHSSgetCurrSequenceIndex() == 0) &&
        (sync_send_interval <= (current_us - SyncPacketNextSend)))
    {
        GenerateSyncPacketData(tx_buffer);
        SyncPacketNextSend = current_us;
    }
    else if (tlm_send)
    {
        /* send tlm packet if needed */
        if (rc_ch.tlm_send(tx_buffer, msp_packet_tx))
        {
            msp_packet_tx.reset();
            tlm_send = 0;
        }
    }
    else
    {
        rc_ch.get_packed_data(tx_buffer);
    }

    // Calculate the CRC
    tx_buffer[7] = CalcCRC(tx_buffer, 7) + CRCCaesarCipher;
    // Enable PA
    PowerMgmt.pa_on();
    //delayMicroseconds(random(0, 200));
    // Send data to rf
    //if (random(0, 99) < 60)
    Radio.TXnb(tx_buffer, 8, freq);
exit_rx_send:
    // Increase TX counter
    HandleFHSS_TX();
}

///////////////////////////////////////

static void ParamWriteHandler(uint8_t const *msg, uint16_t len)
{
    // Called from UART handling loop (main loop)
    if (len != 2)
        return;

    uint8_t value = msg[1];
    uint8_t modified = 0;

    switch (msg[0])
    {
        case 0: // send all params
            break;

        case 1:
            // set air rate
            if (RATE_MAX > value)
                modified |= SetRFLinkRate(value) ? (1 << 1) : 0;
            DEBUG_PRINT("Rate: ");
            DEBUG_PRINTLN(ExpressLRS_currAirRate->enum_rate);
            break;

        case 2:
            // set TLM interval
            modified = TLMinterval;
            if (value == TLM_RATIO_DEFAULT)
            {
                // Default requested
                TLMinterval = ExpressLRS_currAirRate->TLMinterval;
            }
            else if (value < TLM_RATIO_MAX)
            {
                TLMinterval = value;
            }

            if (modified != TLMinterval)
            {
#if PLATFORM_ESP32
                modified = 0;
#else
                modified = (1 << 2);
#endif
                if (TLM_RATIO_NO_TLM < TLMinterval)
                    tlm_check_ratio = TLMratioEnumToValue(TLMinterval) - 1;
                else
                    tlm_check_ratio = 0;
            }
            DEBUG_PRINT("TLM: ");
            DEBUG_PRINTLN(TLMinterval);
            break;

        case 3:
            // set TX power
            modified = PowerMgmt.currPower();
            PowerMgmt.setPower((PowerLevels_e)value);
            crsf.LinkStatistics.downlink_TX_Power = PowerMgmt.power_to_radio_enum();
            DEBUG_PRINT("Power: ");
            DEBUG_PRINTLN(PowerMgmt.currPower());
#if PLATFORM_ESP32
            modified = 0;
#else
            modified = (modified != PowerMgmt.currPower()) ? (1 << 3) : 0;
#endif
            break;

        case 4:
            // RFFreq
            break;

#if 1
        case 5:
        {
            uint8_t power = msg[3];
            uint8_t crc = 0;
            uint16_t freq = (uint16_t)msg[1] * 8 + msg[2]; // band * 8 + channel
            uint8_t vtx_cmd[] = {(uint8_t)(freq >> 8), (uint8_t)freq, power, (power == 0)};
            uint8_t size = sizeof(vtx_cmd);
            // VTX settings
            msp_packet_tx.reset();
            //msp_packet_tx.makeCommand();
            msp_packet_tx.flags = MSP_VERSION | MSP_STARTFLAG;
            msp_packet_tx.payloadSize = size + 1;
            crc = CalcCRCxor((uint8_t)msp_packet_tx.payloadSize, crc);
            msp_packet_tx.function = MSP_VTX_SET_CONFIG;
            crc = CalcCRCxor((uint8_t)msp_packet_tx.function, crc);
            for (uint8_t iter = 0; iter < size; iter++)
            {
                msp_packet_tx.addByte(vtx_cmd[iter]);
                crc = CalcCRCxor((uint8_t)vtx_cmd[iter], crc);
            }
            msp_packet_tx.addByte(crc);
            tlm_send = 1; // rdy for sending
            break;
        }
#endif

        default:
            break;
    }

    uint8_t resp[5] = {(uint8_t)(ExpressLRS_currAirRate->enum_rate),
                       (uint8_t)(TLMinterval),
                       (uint8_t)(PowerMgmt.currPower()),
                       (uint8_t)(PowerMgmt.maxPower()),
                       0xff};
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_FCC_915)
    resp[4] = 0;
#elif defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_EU_868_R9)
    resp[4] = 1;
#elif defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    resp[4] = 2;
#endif

    crsf.sendLUAresponseToRadio(resp, sizeof(resp));

    if (modified)
    {
        // Save modified values
        pl_config.key = ELRS_EEPROM_KEY;
        pl_config.mode = ExpressLRS_currAirRate->enum_rate;
        pl_config.power = PowerMgmt.currPower();
        pl_config.tlm = TLMinterval;
        platform_config_save(pl_config);

        // and start timer if rate was changed
        if (modified & (1 << 1))
            TxTimer.start();
    }
}

///////////////////////////////////////

static uint8_t SetRFLinkRate(uint8_t rate, uint8_t init) // Set speed of RF link (hz)
{
    // TODO: Protect this by disabling timer/isr...

    const expresslrs_mod_settings_s *const config = get_elrs_airRateConfig(rate);
    if (config == NULL || config == ExpressLRS_currAirRate)
        return 0; // No need to modify, rate is same

    if (!init)
    {
        // Stop timer and put radio into sleep
        TxTimer.stop();
        Radio.StopContRX();
    }

    ExpressLRS_currAirRate = config;
    TxTimer.updateInterval(config->interval); // TODO: Make sure this is equiv to above commented lines

    FHSSsetCurrIndex(0);
    Radio.Config(config->bw, config->sf, config->cr, GetInitialFreq(), 0);
    Radio.SetPreambleLength(config->PreambleLen);
    crsf.setRcPacketRate(config->interval);
    crsf.LinkStatistics.rf_Mode = RATE_GET_OSD_NUM(config->enum_rate);
    if (TLMinterval == TLM_RATIO_DEFAULT) // unknown, so use default
        TLMinterval = config->TLMinterval;
    if (TLM_RATIO_NO_TLM == TLMinterval)
    {
        Radio.RXdoneCallback1 = SX127xDriver::rx_nullCallback;
        Radio.TXdoneCallback1 = SX127xDriver::tx_nullCallback;
        // Set connected if telemetry is not used
        connectionState = STATE_connected;
        //sync_send_interval = SYNC_PACKET_SEND_INTERVAL_RX_CONN;
        tlm_check_ratio = 0;
    }
    else
    {
        Radio.RXdoneCallback1 = ProcessTLMpacket;
        Radio.TXdoneCallback1 = HandleTLM;
        connectionState = STATE_disconnected;
        //sync_send_interval = SYNC_PACKET_SEND_INTERVAL_RX_LOST;
        tlm_check_ratio = TLMratioEnumToValue(TLMinterval) - 1;
    }
    sync_send_interval = config->syncInterval;

    platform_connection_state(connectionState);

    return 1;
}

static void hw_timer_init(void)
{
    red_led_state = 1;
    platform_set_led(1);
    TxTimer.init();
    TxTimer.start();
}
static void hw_timer_stop(void)
{
    red_led_state = 0;
    platform_set_led(0);
    TxTimer.stop();
}

static void rc_data_cb(crsf_channels_t const *const channels)
{
    rc_ch.processChannels(channels);
}

/* OpenTX sends v1 MSPs */
static void msp_data_cb(uint8_t const *const input)
{
    if (tlm_send)
        return;

    /* process MSP packet from radio
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

    tlm_send = 1; // rdy for sending
}

void setup()
{
    PowerLevels_e power;
    msp_packet_tx.reset();
    msp_packet_rx.reset();

    DEBUG_PRINTLN("ExpressLRS TX Module...");
    CrsfSerial.Begin(CRSF_TX_BAUDRATE_FAST);

    platform_setup();
    platform_config_load(pl_config);
    current_rate_config = pl_config.mode % RATE_MAX;
    power = (PowerLevels_e)(pl_config.power % PWR_UNKNOWN);
    TLMinterval = pl_config.tlm;
    platform_mode_notify();

    crsf.connected = hw_timer_init; // it will auto init when it detects UART connection
    crsf.disconnected = hw_timer_stop;
    crsf.ParamWriteCallback = ParamWriteHandler;
    crsf.RCdataCallback1 = rc_data_cb;
    crsf.MspCallback = msp_data_cb;

    TxTimer.callbackTock = &SendRCdataToRF;

    //FHSSrandomiseFHSSsequence();

#if defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
    Radio.RFmodule = RFMOD_SX1278;
#else
    Radio.RFmodule = RFMOD_SX1276;
#endif
    Radio.SetPins(GPIO_PIN_RST, GPIO_PIN_DIO0, GPIO_PIN_DIO1);
    Radio.SetSyncWord(getSyncWord());
    Radio.Begin(GPIO_PIN_TX_ENABLE, GPIO_PIN_RX_ENABLE);

    PowerMgmt.Begin();
    PowerMgmt.defaultPower(power);
    crsf.LinkStatistics.downlink_TX_Power = PowerMgmt.power_to_radio_enum();

    SetRFLinkRate(current_rate_config, 1);

    crsf.Begin();
}

void loop()
{
    uint8_t can_send;

    if (rx_buffer_handle)
    {
        process_rx_buffer();
        rx_buffer_handle = 0;
        goto exit_loop;
    }

    if (TLM_RATIO_NO_TLM < TLMinterval)
    {
        uint32_t current_ms = millis();

        if (connectionState > STATE_disconnected &&
            RX_CONNECTION_LOST_TIMEOUT < (current_ms - LastPacketRecvMillis))
        {
            connectionState = STATE_disconnected;
            //sync_send_interval = SYNC_PACKET_SEND_INTERVAL_RX_LOST;
            platform_connection_state(STATE_disconnected);
            platform_set_led(red_led_state);
        }
        else if (LQ_CALCULATE_INTERVAL <= (current_ms - PacketRateNextCheck))
        {
            PacketRateNextCheck = current_ms;

            // Calc LQ based on good tlm packets and receptions done
            uint8_t rx_cnt = expected_tlm_counter;
            uint32_t tlm_cnt = recv_tlm_counter;
            expected_tlm_counter = recv_tlm_counter = 0; // Clear RX counter
            if (rx_cnt)
                downlink_linkQuality = (tlm_cnt * 100u) / rx_cnt;
            else
                // failure value??
                downlink_linkQuality = 0;
        }
        else if (connectionState == STATE_connected &&
                 TLM_REPORT_INTERVAL <= (current_ms - TlmSentToRadioTime))
        {
            TlmSentToRadioTime = current_ms;
            crsf.LinkStatistics.downlink_Link_quality = downlink_linkQuality;
            crsf.LinkStatisticsSend();
            crsf.BatterySensorSend();
        }
    }

    // Process CRSF packets from TX
    can_send = crsf.handleUartIn(rx_buffer_handle);
    // Send MSP resp if allowed or packet ready
    if (can_send && msp_packet_rx.iterated())
    {
        crsf.sendMspPacketToRadio(msp_packet_rx);
        msp_packet_rx.reset();
    }

    platform_loop(connectionState);

exit_loop:
    platform_wd_feed();
}

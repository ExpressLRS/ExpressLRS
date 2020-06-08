#include <Arduino.h>
#include "utils.h"
#include "common.h"
#include "LoRa_SX127x.h"
#include "CRSF_TX.h"
#include "FHSS.h"
#include "targets.h"
#include "POWERMGNT.h"
#include "HwTimer.h"
#include "debug_elrs.h"
#include "rc_channels.h"
#include "LowPassFilter.h"
#include <stdlib.h>

static uint8_t SetRFLinkRate(uint8_t rate, uint8_t init = 0);

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT        1500U // After 1500ms of no TLM response consider that slave has lost connection
#define LQ_CALCULATE_INTERVAL             500u
#ifndef TLM_REPORT_INTERVAL
#define TLM_REPORT_INTERVAL               300u
#endif
#define TX_POWER_UPDATE_PERIOD            1500
//#define SYNC_PACKET_SEND_INTERVAL_RX_LOST 150000u //  250000u
//#define SYNC_PACKET_SEND_INTERVAL_RX_CONN 350000u // 1500000u
#define SYNC_PACKET_SEND_INTERVAL_RX_LOST 250000u
#define SYNC_PACKET_SEND_INTERVAL_RX_CONN SYNC_PACKET_SEND_INTERVAL_RX_LOST

///////////////////

/// define some libs to use ///
SX127xDriver Radio(RadioSpi);
CRSF_TX crsf(CrsfSerial);
RcChannels DRAM_ATTR rc_ch;
POWERMGNT PowerMgmt(Radio);

volatile uint32_t DRAM_ATTR _rf_rxtx_counter = 0;
static volatile uint8_t DMA_ATTR rx_buffer[8];
static volatile uint8_t DRAM_ATTR rx_buffer_handle = 0;
static volatile uint8_t red_led_state = 0;

static uint16_t DRAM_ATTR CRCCaesarCipher = 0;

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
static uint32_t DRAM_ATTR LastPacketRecvMillis = 0;
volatile connectionState_e DRAM_ATTR connectionState = STATE_disconnected;

//////////// TELEMETRY /////////
static volatile uint32_t DMA_ATTR expected_tlm_counter = 0;
static uint32_t DMA_ATTR recv_tlm_counter = 0;
static volatile uint32_t DRAM_ATTR tlm_check_ratio = 0;
static volatile uint_fast8_t DRAM_ATTR TLMinterval = 0;
static mspPacket_t msp_packet_tx;
static mspPacket_t msp_packet_rx;
static volatile uint_fast8_t DRAM_ATTR tlm_msp_send = 0;
static uint32_t DRAM_ATTR TlmSentToRadioTime = 0;
static LPF LPF_dyn_tx_power(3);
static uint32_t dyn_tx_updated = 0;

//////////// LUA /////////

///////////////////////////////////////

int8_t tx_tlm_change_interval(uint8_t const value)
{
    uint8_t const current = TLMinterval;
    if (value == TLM_RATIO_DEFAULT)
    {
        // Default requested
        TLMinterval = ExpressLRS_currAirRate->TLMinterval;
    }
    else if (value < TLM_RATIO_MAX)
    {
        TLMinterval = value;
    }

    if (current != TLMinterval)
    {
        if (TLM_RATIO_NO_TLM < TLMinterval)
            tlm_check_ratio = TLMratioEnumToValue(TLMinterval) - 1;
        else
            tlm_check_ratio = 0;
        DEBUG_PRINT("TLM: ");
        DEBUG_PRINTLN(TLMinterval);
        return 0;
    }
    return -1;
}

void tx_tlm_disable_enable(uint8_t enable)
{
    if (enable)
        tx_tlm_change_interval(TLM_RATIO_DEFAULT);
    else
        tx_tlm_change_interval(TLM_RATIO_NO_TLM);
}

int8_t tx_tlm_toggle(void)
{
    /* Toggle TLM between NO_TLM and DEFAULT */
    uint8_t tlm = (TLMinterval == TLM_RATIO_NO_TLM) ? TLM_RATIO_DEFAULT : TLM_RATIO_NO_TLM;
    tx_tlm_change_interval(tlm);
    return (TLMinterval != TLM_RATIO_NO_TLM);
}

///////////////////////////////////////

static void process_rx_buffer()
{
    uint32_t ms = millis();
    const uint16_t crc = CalcCRC16((uint8_t*)rx_buffer, 6, CRCCaesarCipher);
    const uint16_t crc_in = ((uint16_t)(rx_buffer[6] & 0x3f) << 8) + rx_buffer[7];
    uint8_t type = TYPE_EXTRACT(rx_buffer[6]);

    if (crc_in != (crc & 0x3FFF))
    {
        DEBUG_PRINT("!C");
        return;
    }

    connectionState = STATE_connected;
    //sync_send_interval = SYNC_PACKET_SEND_INTERVAL_RX_CONN;
    platform_connection_state(STATE_connected);
    platform_set_led(0);
    LastPacketRecvMillis = ms;
    recv_tlm_counter++;

    switch (type)
    {
        case DL_PACKET_TLM_MSP:
        {
            DEBUG_PRINTLN("DL MSP junk");
            rc_ch.tlm_receive(rx_buffer, msp_packet_rx);
            break;
        }
        case DL_PACKET_TLM_LINK:
        {
            crsf.LinkStatisticsExtract(rx_buffer,
                                       Radio.LastPacketSNR,
                                       Radio.LastPacketRSSI);

            // Check RSSI and update TX power if needed
            int8_t rssi = LPF_dyn_tx_power.update((int8_t)crsf.LinkStatistics.uplink_RSSI_1);
            if (TX_POWER_UPDATE_PERIOD <= (ms - dyn_tx_updated)) {
                dyn_tx_updated = ms;
                if (-75 < rssi) {
                    PowerMgmt.decPower();
                } else if (-95 > rssi) {
                    PowerMgmt.incPower();
                }
            }
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
    sync->fhssIndex = FHSSgetCurrIndex();
    sync->rxtx_counter = _rf_rxtx_counter;
    sync->air_rate = ExpressLRS_currAirRate->enum_rate;
    sync->tlm_interval = TLMinterval;
    sync->uid3 = UID[3];
    sync->uid4 = UID[4];
    sync->uid5 = UID[5];
    output[6] = TYPE_PACK(UL_PACKET_SYNC);
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
        HandleFHSS_TX();
        return;
    }

    freq = FHSSgetCurrFreq();

    //only send sync when its time and only on channel 0;
    if ((FHSSgetCurrSequenceIndex() == 0) &&
        (sync_send_interval <= (uint32_t)(current_us - SyncPacketNextSend)))
    {
        GenerateSyncPacketData(tx_buffer);
        SyncPacketNextSend = current_us;
    }
    else if ((tlm_msp_send == 1) && (msp_packet_tx.type == MSP_PACKET_TLM_OTA))
    {
        /* send tlm packet if needed */
        if (rc_ch.tlm_send(tx_buffer, msp_packet_tx) || msp_packet_tx.error)
        {
            msp_packet_tx.reset();
            tlm_msp_send = 0;
            DEBUG_PRINTLN("MSP sent");
        }
    }
    else
    {
        rc_ch.get_packed_data(tx_buffer);
    }

    // Calculate the CRC
    uint16_t crc = CalcCRC16(tx_buffer, 6, CRCCaesarCipher);
    tx_buffer[6] += (crc >> 8) & 0x3F;
    tx_buffer[7] = (crc & 0xFF);
    // Enable PA
    PowerMgmt.pa_on();
    // Debugging
    //delayMicroseconds(random(0, 400)); // 300 ok
    //if (random(0, 99) < 55) tx_buffer[1] = 0;
    // Send data to rf
    Radio.TXnb(tx_buffer, 8, freq);
    // Increase TX counter
    HandleFHSS_TX();
}

///////////////////////////////////////

static int8_t SettingsCommandHandle(uint8_t const cmd, uint8_t const len, uint8_t const *msg, uint8_t *out)
{
    uint8_t modified = 0;
    uint8_t value = msg[0];

    switch (cmd)
    {
        case 0: // send all params
            break;

        case 1:
            if (len != 1)
                return -1;

            // set air rate
            if (RATE_MAX > value)
                modified |= SetRFLinkRate(value) ? (1 << 1) : 0;
            DEBUG_PRINT("Rate: ");
            DEBUG_PRINTLN(ExpressLRS_currAirRate->enum_rate);
            break;

        case 2:
            // set TLM interval
            if (len != 1)
                return -1;

            if (tx_tlm_change_interval(value) >= 0)
            {
#ifndef PLATFORM_ESP32
                modified = (1 << 2);
#endif
            }
            //DEBUG_PRINT("TLM: ");
            //DEBUG_PRINTLN(TLMinterval);
            break;

        case 3:
            // set TX power
            if (len != 1)
                return -1;
            modified = PowerMgmt.currPower();
            PowerMgmt.setPower((PowerLevels_e)value);
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
            if (len != 3)
                return -1;
            uint8_t power = msg[2];
            uint8_t crc = 0;
            uint16_t freq = (uint16_t)msg[0] * 8 + msg[1]; // band * 8 + channel
            uint8_t vtx_cmd[] = {(uint8_t)(freq >> 8), (uint8_t)freq, power, (power == 0)};
            uint8_t size = sizeof(vtx_cmd);
            // VTX settings
            msp_packet_tx.reset();
            //msp_packet_tx.makeCommand();
            msp_packet_tx.type = MSP_PACKET_TLM_OTA;
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
            tlm_msp_send = 1; // rdy for sending

            DEBUG_PRINT("VTX MSP - freq:");
            DEBUG_PRINT(freq);
            DEBUG_PRINT(" pwr:");
            DEBUG_PRINTLN(power);
            break;
        }
#endif

        default:
            return -1;
    }


    // Fill response
    if (out) {
        out[0] = (uint8_t)(ExpressLRS_currAirRate->enum_rate);
        out[1] = (uint8_t)(TLMinterval);
        out[2] = (uint8_t)(PowerMgmt.currPower());
        out[3] = (uint8_t)(PowerMgmt.maxPowerGet());
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_FCC_915)
        out[4] = 0;
#elif defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_EU_868_R9)
        out[4] = 1;
#elif defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
        out[4] = 2;
#else
        out[4] = 0xff;
#endif
    }

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

    return 0;
}

static void ParamWriteHandler(uint8_t const *msg, uint16_t len)
{
    // Called from UART handling loop (main loop)
    uint8_t resp[5];
    if (0 > SettingsCommandHandle(msg[0], len, &msg[1], resp))
        return;
    crsf.sendLUAresponseToRadio(resp, sizeof(resp));
}

#ifdef CTRL_SERIAL
static uint8_t cmd_buffer[128];
static uint8_t * cmd_next = cmd_buffer;

static void CtrlCommandHandler(uint8_t *msg, uint16_t len)
{
    // Check header: ELRS
    if (5 < len && msg[0] == 'E' && msg[1] == 'L' && msg[2] == 'R' && msg[3] == 'S') {
        if (0 > SettingsCommandHandle(msg[4], (len - 6), &msg[5], msg)) {
            CTRL_SERIAL.println("SETTING FAILED!");
            return;
        }
        // Send response
        CTRL_SERIAL.print("ELRS_RESP_");
        CTRL_SERIAL.write(msg, 5); //  payload 5B
        CTRL_SERIAL.write('\n');
    } else if (msg[0] == 'M' && msg[1] == 'S' && msg[2] == 'P') {
        // handle MSP message input
    }
}
#endif /* CTRL_SERIAL */

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

    LPF_dyn_tx_power.init(-55);

    ExpressLRS_currAirRate = config;
    TxTimer.updateInterval(config->interval); // TODO: Make sure this is equiv to above commented lines

    FHSSsetCurrIndex(0);
    Radio.Config(config->bw, config->sf, config->cr, GetInitialFreq(), 0);
    Radio.SetPreambleLength(config->PreambleLen);
    crsf.setRcPacketRate(config->interval);
    crsf.LinkStatistics.rf_Mode = RATE_GET_OSD_NUM(config->enum_rate);
    if (TLMinterval == TLM_RATIO_DEFAULT) // unknown, so use default
        TLMinterval = config->TLMinterval;
    if (TLM_RATIO_NO_TLM == TLMinterval || TLM_RATIO_MAX <= TLMinterval)
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
    if (tlm_msp_send)
        return;

    DEBUG_PRINTLN("MSP from radio rcvd");

    /* process MSP packet from radio
     *  [0] header: seq&0xF,
     *  [1] payload size
     *  [2] function
     *  [3...] payload + crc
     */
    mspHeaderV1_t *hdr = (mspHeaderV1_t *)input;

    if (sizeof(msp_packet_tx.payload) < hdr->payloadSize)
        /* too big, ignore */
        return;

    msp_packet_tx.reset(hdr);
    msp_packet_tx.type = MSP_PACKET_TLM_OTA;
    if (0 < hdr->payloadSize)
        volatile_memcpy(msp_packet_tx.payload,
                        hdr->payload,
                        hdr->payloadSize);

    tlm_msp_send = 1; // rdy for sending
}

void setup()
{
    PowerLevels_e power;
    msp_packet_tx.reset();
    msp_packet_rx.reset();

    CRCCaesarCipher = CalcCRC16(UID, sizeof(UID), 0);

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
    PowerMgmt.setPower(power);

    SetRFLinkRate(current_rate_config, 1);

#ifdef CTRL_SERIAL
    // populate current configuration
    uint8_t cmd[] = {'E', 'L', 'R', 'S', 0, 0, '\n'};
    CtrlCommandHandler(cmd, sizeof(cmd));
#endif /* CTRL_SERIAL */

    crsf.Begin();
}

void loop()
{
    uint8_t can_send;

    if (rx_buffer_handle)
    {
        process_rx_buffer();
        rx_buffer_handle = 0;
        platform_wd_feed();
    }

    if (TLM_RATIO_NO_TLM < TLMinterval)
    {
        uint32_t current_ms = millis();

        if (connectionState > STATE_disconnected &&
            RX_CONNECTION_LOST_TIMEOUT < (uint32_t)(current_ms - LastPacketRecvMillis))
        {
            connectionState = STATE_disconnected;
            //sync_send_interval = SYNC_PACKET_SEND_INTERVAL_RX_LOST;
            platform_connection_state(STATE_disconnected);
            platform_set_led(red_led_state);
        }
        else if (connectionState == STATE_connected &&
                 TLM_REPORT_INTERVAL <= (uint32_t)(current_ms - TlmSentToRadioTime))
        {
            TlmSentToRadioTime = current_ms;

            // Calc LQ based on good tlm packets and receptions done
            uint8_t rx_cnt = expected_tlm_counter;
            uint32_t tlm_cnt = recv_tlm_counter;
            expected_tlm_counter = recv_tlm_counter = 0; // Clear RX counter
            if (rx_cnt)
                crsf.LinkStatistics.downlink_Link_quality = (tlm_cnt * 100u) / rx_cnt;
            else
                // failure value??
                crsf.LinkStatistics.downlink_Link_quality = 0;

            crsf.LinkStatistics.uplink_TX_Power = PowerMgmt.power_to_radio_enum();
            crsf.LinkStatisticsSend();
            crsf.BatterySensorSend();
        }
    }

    // Process CRSF packets from TX
    can_send = crsf.handleUartIn(rx_buffer_handle);

    if (!rx_buffer_handle) {
        // Send MSP resp if allowed and packet ready
        if (can_send && msp_packet_rx.iterated())
        {
            DEBUG_PRINTLN("DL MSP rcvd => radio");
            if (!msp_packet_rx.error)
                crsf.sendMspPacketToRadio(msp_packet_rx);
            msp_packet_rx.reset();
        }
#ifdef CTRL_SERIAL
        else if (CTRL_SERIAL.available()) {
            platform_wd_feed();
            uint8_t in = CTRL_SERIAL.read();
            *cmd_next++ = in;
            uint8_t cnt = (cmd_next - cmd_buffer);
            if (3 < cnt && in == '\n') {
                CtrlCommandHandler(cmd_buffer, cnt);
                cmd_buffer[0] = 0;
                cmd_next = cmd_buffer;
            } else if (sizeof(cmd_buffer) <= cnt) {
                cmd_buffer[0] = 0;
                cmd_next = cmd_buffer;
            }
        }
#endif /* CTRL_SERIAL */
    }

    platform_loop(connectionState);
    platform_wd_feed();
}

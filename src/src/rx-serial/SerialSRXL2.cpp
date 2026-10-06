#if (defined(TARGET_RX) && defined(PLATFORM_ESP32)) || defined(SRXL2_ADAPTER_TEST)
#include "SerialSRXL2.h"
#include "CRSFRouter.h"
#include "common.h"
#include <cstring>
#if defined(PLATFORM_ESP32)
#include "config.h"
#include "driver/gpio.h"
#include "hal/uart_ll.h"
#include "esp32-hal-matrix.h"
#include "soc/gpio_sig_map.h"
#if defined(SRXL2_DIAGNOSTICS)
#include <ArduinoJson.h>
#endif
#if defined(CONFIG_IDF_TARGET_ESP32)
#include "driver/periph_ctrl.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "hal/gpio_ll.h"
#endif
#endif

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(SRXL2_INSTALL_TX_IRQ)
#define SRXL2_IRQ_TX
#endif

#ifndef SRXL2_HARDWARE_RX_BUSY
#if defined(CONFIG_IDF_TARGET_ESP32)
#define SRXL2_HARDWARE_RX_BUSY() (uart_ll_get_rxfifo_len(UART_LL_GET_HW(0)) != 0 || \
    UART_LL_GET_HW(0)->status.st_urx_out != 0 || gpio_get_level(gpio_num_t(pin)) == 0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
// C3/S3 keep the receive state machine in a separate register.
#define SRXL2_HARDWARE_RX_BUSY() (uart_ll_get_rxfifo_len(UART_LL_GET_HW(0)) != 0 || \
    UART_LL_GET_HW(0)->fsm_status.st_urx_out != 0 || gpio_get_level(gpio_num_t(pin)) == 0)
#elif defined(PLATFORM_ESP32)
#define SRXL2_HARDWARE_RX_BUSY() true // unsupported SoCs cannot start bus traffic
#else
#define SRXL2_HARDWARE_RX_BUSY() false
#endif
#endif

#ifndef SRXL2_BEGIN_TX
#if defined(CONFIG_IDF_TARGET_ESP32)
#define SRXL2_BEGIN_TX() do { \
    gpio_set_level(gpio_num_t(pin), 1); \
    gpio_set_direction(gpio_num_t(pin), GPIO_MODE_INPUT_OUTPUT); \
    pinMatrixOutAttach(pin, U2TXD_OUT_IDX, false, false); \
} while (0)
#define SRXL2_RELEASE_TX() gpio_ll_output_disable(&GPIO, gpio_num_t(driver->pin))
#else
#define SRXL2_BEGIN_TX() ((void)0)
#define SRXL2_RELEASE_TX() ((void)0)
#endif
#endif

#ifndef SRXL2_MODE_ACTIVE
#if defined(TARGET_RX)
#define SRXL2_MODE_ACTIVE() (config.GetSerialProtocol() == PROTOCOL_SRXL2 && !firmwareOptions.is_airport)
#else
#define SRXL2_MODE_ACTIVE() true
#endif
#endif

static volatile uint32_t srxl2RFGeneration = 0;

void ICACHE_RAM_ATTR SerialSRXL2::onRFReset()
{
    ++srxl2RFGeneration;
}

SerialSRXL2::SerialSRXL2(Stream *output, Stream *input, int8_t txPin)
    : SerialIO(output, input), pin(txPin), inputPort(input)
{
    uint32_t uid = 0x12345678;
#if defined(PLATFORM_ESP32)
    const uint64_t mac = ESP.getEfuseMac();
    uid = uint32_t(mac) ^ uint32_t(mac >> 32);
    gpio_config_t config{};
    config.pin_bit_mask = uint64_t(1) << pin;
#if defined(CONFIG_IDF_TARGET_ESP32)
    config.mode = GPIO_MODE_INPUT;
#else
    config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
#endif
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    pinMatrixInAttach(pin, U0RXD_IN_IDX, false);
#if defined(CONFIG_IDF_TARGET_ESP32)
    // UART0 retains Arduino's RX buffering. Own the otherwise unused UART2 TX
    // so a short IRAM interrupt can release the bus without task scheduling.
    txReady = false;
    if (!uart_is_driver_installed(UART_NUM_2))
    {
        periph_module_enable(PERIPH_UART2_MODULE);
        periph_module_reset(PERIPH_UART2_MODULE);
        auto hw = UART_LL_GET_HW(2);
        uart_ll_disable_intr_mask(hw, UART_LL_INTR_MASK);
        uart_ll_clr_intsts_mask(hw, UART_LL_INTR_MASK);
        uart_ll_set_sclk(hw, UART_SCLK_APB);
        uart_ll_set_baudrate(hw, 115200);
        uart_ll_set_data_bit_num(hw, UART_DATA_8_BITS);
        uart_ll_set_parity(hw, UART_PARITY_DISABLE);
        uart_ll_set_stop_bits(hw, UART_STOP_BITS_1);
        uart_ll_set_tx_idle_num(hw, 0);
        uart_ll_set_hw_flow_ctrl(hw, UART_HW_FLOWCTRL_DISABLE, 0);
        txReady = esp_intr_alloc(ETS_UART2_INTR_SOURCE, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
            onTxDone, this, &txInterrupt) == ESP_OK;
        if (!txReady) periph_module_disable(PERIPH_UART2_MODULE);
    }
#else
    pinMatrixOutAttach(pin, U0TXD_OUT_IDX, false, false);
#endif
#endif
#if defined(SRXL2_INSTALL_TX_IRQ)
    txReady = SRXL2_INSTALL_TX_IRQ(onTxDone, this);
#endif
    link.reset(uid, micros());
    lastPublished = micros();
    generation = srxl2RFGeneration;
    lastHardwareReceive = micros();
    crsfBatterySensorDetected = false;
}

SerialSRXL2::~SerialSRXL2()
{
#if defined(CONFIG_IDF_TARGET_ESP32)
    if (txInterrupt)
    {
        uart_ll_disable_intr_mask(UART_LL_GET_HW(2), UART_LL_INTR_MASK);
        esp_intr_free(txInterrupt);
        periph_module_disable(PERIPH_UART2_MODULE);
    }
#endif
#if defined(SRXL2_REMOVE_TX_IRQ)
    SRXL2_REMOVE_TX_IRQ();
#endif
#if defined(PLATFORM_ESP32)
    gpio_set_direction(gpio_num_t(pin), GPIO_MODE_INPUT);
#endif
    crsfBatterySensorDetected = false;
}

bool SerialSRXL2::controlAllowed() const
{
    return SRXL2_MODE_ACTIVE() && !failsafe && connectionState == connected && connectionHasModelMatch && teamraceHasModelMatch &&
        ChannelData[2] != CRSF_CHANNEL_VALUE_UNSET;
}

bool SerialSRXL2::synchronizeGeneration()
{
    const uint32_t current = srxl2RFGeneration;
    if (generation == current) return true;
    generation = current;
    link.setControlPermission(false);
    skipNextFrame = true;
    return false;
}

uint32_t SerialSRXL2::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *)
{
    synchronizeGeneration();
    // Upstream's shared snapshot replaces UNSET with minimum. Read only our
    // throttle from the raw channel state, without changing other protocols.
    noInterrupts();
    const uint32_t throttle = ChannelData[2];
    const bool allowed = controlAllowed();
    interrupts();
#if defined(SRXL2_DIAGNOSTICS)
    // Preserve the RF state when Lua switches into Wi-Fi without a reboot.
    if (connectionState != wifiUpdate)
    {
        diagnostics.rfConnected = connectionState == connected;
        diagnostics.allowed = allowed;
        diagnostics.modelMatch = connectionHasModelMatch;
        diagnostics.teamMatch = teamraceHasModelMatch;
        diagnostics.failsafe = failsafe;
        diagnostics.ch3 = throttle;
        if (frameAvailable)
        {
            ++diagnostics.frames;
            if (throttle != CRSF_CHANNEL_VALUE_UNSET)
            {
                if (throttle < diagnostics.ch3Min) diagnostics.ch3Min = throttle;
                if (throttle > diagnostics.ch3Max) diagnostics.ch3Max = throttle;
            }
        }
    }
#endif
    link.setControlPermission(allowed);
    if (frameAvailable && skipNextFrame) skipNextFrame = false;
    else if (allowed && frameAvailable) link.setThrottle(throttle, micros());
    else if (frameMissed) link.missedFrame();
    return 1;
}

void SerialSRXL2::completeTransmission(uint32_t now)
{
    if (!transmitting) return;
#if defined(SRXL2_IRQ_TX)
#if defined(SRXL2_POLL_TX_IRQ)
    SRXL2_POLL_TX_IRQ();
#endif
    if (!txComplete) return;
    now = txEnded;
#elif defined(PLATFORM_ESP32)
    // The factory disables software TX buffering; observe both FIFO and shifter.
    if (!uart_ll_is_tx_idle(UART_LL_GET_HW(0))) return;
#endif
    transmitting = false;
    link.transmitted(now);
}

void ICACHE_RAM_ATTR SerialSRXL2::onTxDone(void *argument)
{
    auto driver = static_cast<SerialSRXL2 *>(argument);
    SRXL2_RELEASE_TX(); // Release first: the ESC can start its reply immediately.
#if defined(CONFIG_IDF_TARGET_ESP32)
    uart_ll_disable_intr_mask(UART_LL_GET_HW(2), UART_INTR_TX_DONE);
    uart_ll_clr_intsts_mask(UART_LL_GET_HW(2), UART_INTR_TX_DONE);
    driver->txEnded = uint32_t(esp_timer_get_time());
#else
    driver->txEnded = micros();
#endif
    driver->txComplete = true;
#if defined(SRXL2_DIAGNOSTICS)
    ++driver->diagnostics.txDone;
#endif
}

void SerialSRXL2::processBytes(uint8_t *bytes, uint16_t size)
{
    const uint32_t now = micros();
    completeTransmission(now);
#if defined(SRXL2_DIAGNOSTICS)
    for (unsigned i = 0; i < size; ++i)
        diagnostics.rxTail[diagnostics.rxBytes++ % sizeof(diagnostics.rxTail)] = bytes[i];
#endif
    for (unsigned i = 0; i < size; ++i) link.receive(bytes[i], now);
}

void SerialSRXL2::sendQueuedData(uint32_t maxBytesToSend)
{
    const uint32_t now = micros();
    synchronizeGeneration();
    link.setControlPermission(controlAllowed());
    completeTransmission(now);
    const bool receivePending = inputPort->available() > 0 || SRXL2_HARDWARE_RX_BUSY();
    if (receivePending) lastHardwareReceive = now;
    if (txReady && !transmitting && !receivePending && uint32_t(now - lastHardwareReceive) >= 174 && maxBytesToSend >= 16)
    {
        SRXL2::Packet packet{};
        if (link.nextPacket(now, packet))
        {
            SRXL2_BEGIN_TX();
            txComplete = false;
            transmitting = true;
#if defined(SRXL2_DIAGNOSTICS)
            ++diagnostics.txPackets;
            if (connectionState == connected)
            {
                diagnostics.lastRfTxLength = min(unsigned(packet.length), unsigned(sizeof(diagnostics.lastRfTx)));
                std::memcpy(diagnostics.lastRfTx, packet.bytes, diagnostics.lastRfTxLength);
            }
#endif
#if defined(CONFIG_IDF_TARGET_ESP32)
            // Only FIFO loading/IRQ arming is atomic; RF runs during transmission.
            // ponytail: current 14/16-byte packets fit the FIFO; add refill IRQs if they grow.
            noInterrupts();
            uart_ll_clr_intsts_mask(UART_LL_GET_HW(2), UART_INTR_TX_DONE);
            uart_ll_write_txfifo(UART_LL_GET_HW(2), packet.bytes, packet.length);
            uart_ll_ena_intr_mask(UART_LL_GET_HW(2), UART_INTR_TX_DONE);
            interrupts();
#else
            _outputPort->write(packet.bytes, packet.length);
#endif
        }
    }
    publishTelemetry(now);
}

#if defined(SRXL2_DIAGNOSTICS)
void SerialSRXL2::event()
{
    if (connectionState != wifiUpdate || diagnosticPublished) return;
#if defined(SRXL2_DIAGNOSTIC_PUBLISH)
    SRXL2_DIAGNOSTIC_PUBLISH();
#elif defined(PLATFORM_ESP32)
    JsonDocument doc;
    if (deserializeJson(doc, getOptions())) return;
    auto state = doc["srxl2-diagnostics"].to<JsonObject>();
    state["rf_connected_before_wifi"] = diagnostics.rfConnected;
    state["control_allowed_before_wifi"] = diagnostics.allowed;
    state["model_match"] = diagnostics.modelMatch;
    state["team_match"] = diagnostics.teamMatch;
    state["failsafe"] = diagnostics.failsafe;
    state["ch3_raw"] = diagnostics.ch3;
    state["ch3_min"] = diagnostics.ch3Min;
    state["ch3_max"] = diagnostics.ch3Max;
    state["input_frames"] = diagnostics.frames;
    state["tx_ready"] = txReady;
    state["tx_packets"] = diagnostics.txPackets;
    state["tx_completions"] = uint32_t(diagnostics.txDone);
    state["rx_bytes"] = diagnostics.rxBytes;
    state["smart_connected"] = link.connected();
    state["rx_busy"] = SRXL2_HARDWARE_RX_BUSY();
    state["signal_level"] = gpio_get_level(gpio_num_t(pin));
#if defined(CONFIG_IDF_TARGET_ESP32)
    if (txReady) state["tx_fifo_bytes"] = uart_ll_get_txfifo_len(UART_LL_GET_HW(2));
#endif
    const char hex[] = "0123456789abcdef";
    String received, sent;
    const unsigned size = diagnostics.rxBytes < 64 ? diagnostics.rxBytes : 64;
    const unsigned start = diagnostics.rxBytes < 64 ? 0 : diagnostics.rxBytes % 64;
    for (unsigned i = 0; i < size; ++i)
    {
        const uint8_t byte = diagnostics.rxTail[(start + i) % 64];
        received += hex[byte >> 4]; received += hex[byte & 15];
    }
    for (unsigned i = 0; i < diagnostics.lastRfTxLength; ++i)
    {
        const uint8_t byte = diagnostics.lastRfTx[i];
        sent += hex[byte >> 4]; sent += hex[byte & 15];
    }
    state["last_rx_hex"] = received;
    state["last_rf_tx_hex"] = sent;
    // ponytail: reuse the options cache for this capture; Web UI Save can persist the debug key.
    String result;
    serializeJson(doc, result);
    setOptions(result);
#endif
    diagnosticPublished = true;
}
#endif

static void putBE(uint8_t *bytes, uint32_t value, unsigned count)
{
    while (count) { bytes[--count] = value; value >>= 8; }
}

void SerialSRXL2::publishTelemetry(uint32_t now)
{
    const SRXL2::Telemetry values = link.telemetry(now);
    const SRXL2::Reading &current = values.batteryCurrent.valid ? values.batteryCurrent : values.current;
    const bool battery = values.voltage.valid && current.valid && current.value >= 0 && current.value / 100 <= 32767;
    crsfBatterySensorDetected = battery;
    if (uint32_t(now - lastPublished) < 100000) return;

    uint8_t frame[CRSF_MAX_PACKET_LEN] = {};
    uint8_t *payload = frame + sizeof(crsf_header_t);
    // Encode payload bytes explicitly, including network-order 24-bit integers.
    for (unsigned attempt = 0; attempt < 7; ++attempt)
    {
        const unsigned sensor = nextSensor;
        nextSensor = (nextSensor + 1) % 7;
        crsf_frame_type_e type = CRSF_FRAMETYPE_TEMP;
        uint8_t size = 0;
        if (sensor == 0 && battery)
        {
            type = CRSF_FRAMETYPE_BATTERY_SENSOR;
            size = 8;
            putBE(payload, values.voltage.value / 100, 2);
            putBE(payload + 2, current.value / 100, 2);
            putBE(payload + 4, values.consumption.valid ? values.consumption.value : 0, 3);
            payload[7] = 0; // CRSF has no interoperable unavailable percentage marker.
        }
        else if (sensor == 0 && values.voltage.valid && values.voltage.value <= 65535)
        {
            type = CRSF_FRAMETYPE_CELLS;
            size = 3;
            payload[0] = 128;
            putBE(payload + 1, values.voltage.value, 2);
        }
        else if (sensor == 1 && values.rpm.valid && values.rpm.value <= 0x7FFFFF)
        {
            type = CRSF_FRAMETYPE_RPM;
            size = 4;
            payload[0] = 0;
            putBE(payload + 1, values.rpm.value, 3);
        }
        else if (sensor >= 2 && sensor <= 4)
        {
            const SRXL2::Reading *temperatures[] = {&values.temperatureFet,&values.temperatureBec,&values.batteryTemperature};
            const auto &temperature = *temperatures[sensor - 2];
            if (temperature.valid && temperature.value >= -32768 && temperature.value <= 32767)
            {
                size = 3;
                payload[0] = sensor - 2;
                putBE(payload + 1, uint16_t(temperature.value), 2);
            }
        }
        else if (sensor == 5 && values.voltageBec.valid)
        {
            type = CRSF_FRAMETYPE_CELLS;
            size = 3;
            payload[0] = 129;
            putBE(payload + 1, values.voltageBec.value, 2);
        }
        else if (sensor == 6 && values.cellCount)
        {
            bool complete = true;
            for (unsigned i = 0; i < values.cellCount; ++i) complete &= values.cells[i].valid;
            if (complete)
            {
                type = CRSF_FRAMETYPE_CELLS;
                size = 1 + values.cellCount * 2;
                payload[0] = 0;
                for (unsigned i = 0; i < values.cellCount; ++i) putBE(payload + 1 + i * 2, values.cells[i].value, 2);
            }
        }
        if (!size) continue;
        auto header = reinterpret_cast<crsf_header_t *>(frame);
        crsfRouter.SetHeaderAndCrc(header, type, CRSF_FRAME_SIZE(size));
        crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, header);
        lastPublished = now;
        return;
    }
}
#endif

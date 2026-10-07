#if (defined(TARGET_RX) && defined(PLATFORM_ESP32)) || defined(SRXL2_ADAPTER_TEST)
#include "SerialSRXL2.h"
#include "CRSFRouter.h"
#include "common.h"
#include "rxtx_intf.h"
#include <atomic>
static std::atomic<bool> smartBatteryDetected{false};
bool hasSRXL2BatteryTelemetry() { return smartBatteryDetected.load(std::memory_order_relaxed); }
#if defined(PLATFORM_ESP32)
#include "config.h"
#include "driver/gpio.h"
#include "hal/uart_ll.h"
#include "esp32-hal-matrix.h"
#include "soc/gpio_sig_map.h"
#include "driver/periph_ctrl.h"
#if defined(CONFIG_IDF_TARGET_ESP32)
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
#define SRXL2_HARDWARE_RX_BUSY() (uart_ll_get_rxfifo_len(UART_LL_GET_HW(port)) != 0 || \
    UART_LL_GET_HW(port)->status.st_urx_out != 0 || gpio_get_level(gpio_num_t(pin)) == 0)
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
// C3/S3 keep the receive state machine in a separate register.
#define SRXL2_HARDWARE_RX_BUSY() (uart_ll_get_rxfifo_len(UART_LL_GET_HW(port)) != 0 || \
    UART_LL_GET_HW(port)->fsm_status.st_urx_out != 0 || gpio_get_level(gpio_num_t(pin)) == 0)
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
#define SRXL2_MODE_ACTIVE() (getSRXL2Port() == port && \
    (port == 0 ? GPIO_PIN_RCSIGNAL_TX : getSerial1TxPin()) == pin)
#else
#define SRXL2_MODE_ACTIVE() true
#endif
#endif

static volatile uint32_t srxl2RFGeneration = 0;

#if defined(SRXL2_EARLY_STARTUP_PIN)
#ifndef SRXL2_EARLY_STARTUP_PORT
#define SRXL2_EARLY_STARTUP_PORT 0
#endif
#ifndef SRXL2_EARLY_BEGIN_TX
#define SRXL2_EARLY_BEGIN_TX() do { \
    gpio_set_level(gpio_num_t(pin), 1); \
    gpio_set_direction(gpio_num_t(pin), GPIO_MODE_INPUT_OUTPUT); \
    pinMatrixOutAttach(pin, port == 0 ? U0TXD_OUT_IDX : U1TXD_OUT_IDX, false, false); \
} while (0)
#define SRXL2_EARLY_RELEASE_TX() gpio_set_direction(gpio_num_t(pin), GPIO_MODE_INPUT)
#define SRXL2_EARLY_YIELD() delay(1)
#endif
struct SRXL2StartupState
{
    uint32_t enteredUs = 0, ackEndUs = 0;
    uint8_t port = 0;
    int8_t pin = -1;
    bool handedOff = false;
};
static SRXL2StartupState startupState;
static void listenForEarlyESC(Stream &input, Stream &output, uint32_t uid, SRXL2StartupState &state,
    uint8_t port = SRXL2_EARLY_STARTUP_PORT, int8_t pin = SRXL2_EARLY_STARTUP_PIN,
    const uint8_t *prefix = nullptr, uint16_t prefixSize = 0)
{
    if (!state.enteredUs) state.enteredUs = micros();
    state.port = port;
    state.pin = pin;
    SRXL2::Link early;
    early.reset(uid, micros(), true);
    for (unsigned i = 0; i < prefixSize; ++i) early.receive(prefix[i], micros());
    // Queued bytes have no recorded wire-arrival timestamps.
    const int buffered = input.available();
    for (int i = 0; i < buffered; ++i)
    {
        const uint8_t byte = input.read();
        early.receive(byte, micros());
    }
    uint32_t lastHardwareReceive = micros();
    (void)pin; // Native tests substitute the hardware boundary.
    bool wasBusy = false;
    bool acknowledged = false;
    while (uint32_t(micros() - state.enteredUs) < 250000)
    {
        while (input.available() && uint32_t(micros() - state.enteredUs) < 250000)
        {
            const uint8_t byte = input.read();
            const uint32_t now = micros();
            early.receive(byte, now);
        }
        const uint32_t now = micros();
        const bool busy = input.available() > 0 || SRXL2_HARDWARE_RX_BUSY();
        if (busy || wasBusy) lastHardwareReceive = now;
        wasBusy = busy;
        if (!acknowledged && !busy && uint32_t(now - lastHardwareReceive) >= 174)
        {
            SRXL2::Packet packet;
            if (early.nextPacket(now, packet))
            {
                SRXL2_EARLY_BEGIN_TX();
                output.write(packet.bytes, packet.length);
                output.flush(); // The last stop bit must finish before releasing the bus.
                SRXL2_EARLY_RELEASE_TX();
                state.ackEndUs = micros();
                early.transmitted(state.ackEndUs);
                acknowledged = true;
            }
        }
        // ACK only. Normal setup owns the final broadcast and all control packets.
        if (acknowledged && uint32_t(micros() - state.ackEndUs) >= 20000) break;
        SRXL2_EARLY_YIELD();
    }
}
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
static_assert(SRXL2_EARLY_STARTUP_PORT <= 1, "Early Smart port must be 0 or 1");
static_assert(SRXL2_EARLY_STARTUP_PIN >= 0 && SRXL2_EARLY_STARTUP_PIN < 64 &&
    ((SOC_GPIO_VALID_OUTPUT_GPIO_MASK >> SRXL2_EARLY_STARTUP_PIN) & 1), "Invalid early Smart signal pin");
#if SRXL2_EARLY_STARTUP_PORT == 1
#include "esp_private/startup_internal.h"
// Capture on UART1 before Arduino or configuration loading; never drive the bus.
ESP_SYSTEM_INIT_FN(srxl2EarlyReceive, BIT0)
{
    periph_module_enable(PERIPH_UART1_MODULE);
    periph_module_reset(PERIPH_UART1_MODULE);
    auto hw = UART_LL_GET_HW(1);
    uart_ll_disable_intr_mask(hw, UART_LL_INTR_MASK);
    uart_ll_clr_intsts_mask(hw, UART_LL_INTR_MASK);
    uart_ll_set_sclk(hw, UART_SCLK_APB);
    uart_ll_set_baudrate(hw, 115200);
    uart_ll_set_data_bit_num(hw, UART_DATA_8_BITS);
    uart_ll_set_parity(hw, UART_PARITY_DISABLE);
    uart_ll_set_stop_bits(hw, UART_STOP_BITS_1);
    uart_ll_set_hw_flow_ctrl(hw, UART_HW_FLOWCTRL_DISABLE, 0);
    gpio_set_direction(gpio_num_t(SRXL2_EARLY_STARTUP_PIN), GPIO_MODE_INPUT);
    gpio_set_pull_mode(gpio_num_t(SRXL2_EARLY_STARTUP_PIN), GPIO_PULLUP_ONLY);
    pinMatrixInAttach(SRXL2_EARLY_STARTUP_PIN, U1RXD_IN_IDX, false);
}
#endif
extern "C" void initVariant()
{
    startupState.enteredUs = micros();
    uint8_t prefix[128];
    uint16_t prefixSize = 0;
#if SRXL2_EARLY_STARTUP_PORT == 1
    // Installing Arduino's driver may reset the FIFO. Preserve its actual bytes.
    prefixSize = uart_ll_get_rxfifo_len(UART_LL_GET_HW(1));
    if (prefixSize > sizeof(prefix)) prefixSize = sizeof(prefix);
    uart_ll_read_rxfifo(UART_LL_GET_HW(1), prefix, prefixSize);
#endif
    auto &uart = SRXL2_EARLY_STARTUP_PORT == 0 ? Serial : Serial1;
    uart.setTxBufferSize(0);
    uart.begin(115200, SERIAL_8N1, SRXL2_EARLY_STARTUP_PIN, -1, false);
    uart.setRxFIFOFull(1);
    pinMode(SRXL2_EARLY_STARTUP_PIN, INPUT_PULLUP); // Select GPIO IOMUX before routing UART0 TX through the matrix.
    pinMatrixInAttach(SRXL2_EARLY_STARTUP_PIN,
        SRXL2_EARLY_STARTUP_PORT == 0 ? U0RXD_IN_IDX : U1RXD_IN_IDX, false);
    const uint64_t mac = ESP.getEfuseMac();
    listenForEarlyESC(uart, uart, uint32_t(mac) ^ uint32_t(mac >> 32), startupState,
        SRXL2_EARLY_STARTUP_PORT, SRXL2_EARLY_STARTUP_PIN, prefix, prefixSize);
    uart.end();
}
#endif
#endif

void ICACHE_RAM_ATTR SerialSRXL2::onRFReset()
{
    ++srxl2RFGeneration;
}

SerialSRXL2::SerialSRXL2(Stream *output, Stream *input, int8_t txPin, uint8_t serialPort)
    : SerialIO(output, input), pin(txPin), port(serialPort), inputPort(input)
{
    // Receive on the shared signal immediately, without attaching an idle-high
    // UART TX output. The GPIO/UART transmitter below owns bus direction.
    auto &uart = port == 0 ? Serial : Serial1;
    uart.setTxBufferSize(0);
    uart.begin(115200, SERIAL_8N1, pin, -1, false);
    uart.setRxFIFOFull(1);
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
    pinMatrixInAttach(pin, port == 0 ? U0RXD_IN_IDX : U1RXD_IN_IDX, false);
#if defined(CONFIG_IDF_TARGET_ESP32)
    // The selected UART retains Arduino's RX buffering. Own unused UART2 TX
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
    pinMatrixOutAttach(pin, port == 0 ? U0TXD_OUT_IDX : U1TXD_OUT_IDX, false, false);
#endif
#endif
#if defined(SRXL2_INSTALL_TX_IRQ)
    txReady = SRXL2_INSTALL_TX_IRQ(onTxDone, this);
#endif
    link.reset(uid, micros());
#if defined(SRXL2_EARLY_STARTUP_PIN)
    if (txReady && port == startupState.port && pin == startupState.pin && startupState.ackEndUs && !startupState.handedOff)
    {
        // An announcing ESC may suppress its duplicate addressed reply. Its validated
        // startup announcement and completed ACK are enough to send the final broadcast.
        link.finishStartupDiscovery();
        startupState.handedOff = true;
    }
#endif
    lastPublished = micros();
    generation = srxl2RFGeneration;
    lastHardwareReceive = micros();
    smartBatteryDetected.store(false, std::memory_order_relaxed);
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
    smartBatteryDetected.store(false, std::memory_order_relaxed);
}

bool SerialSRXL2::controlAllowed() const
{
    return !stopping && SRXL2_MODE_ACTIVE() && !failsafe && connectionState == connected && connectionHasModelMatch && teamraceHasModelMatch &&
        ChannelData[2] != CRSF_CHANNEL_VALUE_UNSET;
}

bool SerialSRXL2::readyForShutdown(uint32_t now)
{
    if (!stopping)
    {
        stopping = true;
        stopStarted = now;
        stopPacketSent = !txReady || !link.connected();
        link.setControlPermission(false);
    }
    completeTransmission(now);
    // ponytail: final neutral gets 25 ms; a stuck RX line is then released as input.
    return !transmitting && link.busIdle(now) && (stopPacketSent || uint32_t(now - stopStarted) >= 25000);
}

void SerialSRXL2::synchronizeGeneration()
{
    const uint32_t current = srxl2RFGeneration;
    if (generation == current) return;
    generation = current;
    link.setControlPermission(false);
    skipNextFrame = true;
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
    if (!uart_ll_is_tx_idle(UART_LL_GET_HW(port))) return;
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
}

void SerialSRXL2::processBytes(uint8_t *bytes, uint16_t size)
{
    const uint32_t now = micros();
    completeTransmission(now);
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
    if (txReady && !(stopping && stopPacketSent) && !transmitting && !receivePending && uint32_t(now - lastHardwareReceive) >= 174 && maxBytesToSend >= 16)
    {
        SRXL2::Packet packet{};
        if (link.nextPacket(now, packet))
        {
            if (stopping && packet.bytes[1] == 0xCD && packet.bytes[3] == 1) stopPacketSent = true;
            SRXL2_BEGIN_TX();
            txComplete = false;
            transmitting = true;
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


static void putBE(uint8_t *bytes, uint32_t value, unsigned count)
{
    while (count) { bytes[--count] = value; value >>= 8; }
}

void SerialSRXL2::publishTelemetry(uint32_t now)
{
    const SRXL2::Telemetry values = link.telemetry(now);
    const SRXL2::Reading &current = values.batteryCurrent.valid ? values.batteryCurrent : values.current;
    const bool battery = values.voltage.valid && current.valid && current.value >= 0 && current.value / 100 <= 32767;
    smartBatteryDetected.store(battery, std::memory_order_relaxed);
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

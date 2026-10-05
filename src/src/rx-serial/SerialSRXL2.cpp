#if (defined(TARGET_RX) && defined(PLATFORM_ESP32)) || defined(SRXL2_ADAPTER_TEST)
#include "SerialSRXL2.h"
#include "CRSFRouter.h"
#include "common.h"
#if defined(PLATFORM_ESP32)
#include "driver/gpio.h"
#include "hal/uart_ll.h"
#include "esp32-hal-matrix.h"
#include "soc/gpio_sig_map.h"
#endif

#ifndef SRXL2_HARDWARE_RX_BUSY
#if defined(CONFIG_IDF_TARGET_ESP32)
#define SRXL2_HARDWARE_RX_BUSY() (uart_ll_get_rxfifo_len(UART_LL_GET_HW(0)) != 0 || \
    UART_LL_GET_HW(0)->status.st_urx_out != 0 || gpio_get_level(gpio_num_t(pin)) == 0)
#elif defined(PLATFORM_ESP32)
#define SRXL2_HARDWARE_RX_BUSY() true // unsupported SoCs cannot start bus traffic
#else
#define SRXL2_HARDWARE_RX_BUSY() false
#endif
#endif

SerialSRXL2::SerialSRXL2(Stream *output, Stream *input, int8_t txPin)
    : SerialIO(output, input), pin(txPin), inputPort(input)
{
    uint32_t uid = 0x12345678;
#if defined(PLATFORM_ESP32)
    const uint64_t mac = ESP.getEfuseMac();
    uid = uint32_t(mac) ^ uint32_t(mac >> 32);
    gpio_config_t config{};
    config.pin_bit_mask = uint64_t(1) << pin;
    config.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&config);
    // Configure direction first: IDF resets the TX matrix when changing it.
    pinMatrixOutAttach(pin, U0TXD_OUT_IDX, false, false);
    pinMatrixInAttach(pin, U0RXD_IN_IDX, false);
#endif
    // Open-drain idle-high releases the bus and RX can capture an immediate reply.
    link.reset(uid, micros());
    lastPublished = micros();
    generation = serialRCFrames[0].generation;
    lastHardwareReceive = micros();
    crsfBatterySensorDetected = false;
}

SerialSRXL2::~SerialSRXL2()
{
#if defined(PLATFORM_ESP32)
    gpio_set_direction(gpio_num_t(pin), GPIO_MODE_INPUT);
#endif
    crsfBatterySensorDetected = false;
}

bool SerialSRXL2::controlAllowed() const
{
    return !failsafe && connectionState == connected && connectionHasModelMatch && teamraceHasModelMatch;
}

bool SerialSRXL2::synchronizeGeneration()
{
    const uint32_t current = serialRCFrames[0].generation;
    if (generation == current) return true;
    generation = current;
    link.setControlPermission(false);
    return false;
}

uint32_t SerialSRXL2::sendRCFrame(bool frameAvailable, bool, uint32_t *channels)
{
    const bool sameGeneration = synchronizeGeneration();
    const bool allowed = controlAllowed() && channels && channels[2] != CRSF_CHANNEL_VALUE_UNSET &&
        serialRCFrames[0].deliveredGeneration == serialRCFrames[0].generation;
    link.setControlPermission(allowed);
    if (sameGeneration && allowed && frameAvailable) link.setThrottle(channels[2], micros());
    return 1;
}

void SerialSRXL2::completeTransmission(uint32_t now)
{
    if (!transmitting) return;
#if defined(PLATFORM_ESP32)
    // The factory disables software TX buffering; observe both FIFO and shifter.
    if (!uart_ll_is_tx_idle(UART_LL_GET_HW(0))) return;
#endif
    transmitting = false;
    link.transmitted(now);
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
    if (!transmitting && !receivePending && uint32_t(now - lastHardwareReceive) >= 174 && maxBytesToSend >= 16)
    {
        SRXL2::Packet packet{};
        if (link.nextPacket(now, packet))
        {
            _outputPort->write(packet.bytes, packet.length);
            transmitting = true;
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

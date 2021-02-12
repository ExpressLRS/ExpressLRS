#include <Arduino.h>
#include "CRSF.h"
#include "../../lib/FIFO/FIFO.h"
#include "HardwareSerial.h"

//#define DEBUG_CRSF_NO_OUTPUT // debug, don't send RC msgs over UART

#ifdef PLATFORM_ESP32
HardwareSerial SerialPort(1);
HardwareSerial CRSF::Port = SerialPort;
portMUX_TYPE FIFOmux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t xHandleOpenTXsync = NULL;
TaskHandle_t xESP32uartTask = NULL;
TaskHandle_t xESP32uartWDT = NULL;
SemaphoreHandle_t mutexOutFIFO = NULL;
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX) || defined(TARGET_R9M_LITE_PRO_TX) || defined(TARGET_RX_GHOST_ATTO_V1)
HardwareSerial CRSF::Port(USART3);
#if defined(TARGET_R9M_LITE_PRO_TX) || defined(TARGET_RX_GHOST_ATTO_V1)
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#else
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#endif
#endif

GENERIC_CRC8 crsf_crc(CRSF_CRC_POLY);

///Out FIFO to buffer messages///
FIFO SerialOutFIFO;

uint8_t CRSF::CSFR_TXpin_Module = GPIO_PIN_RCSIGNAL_TX;
uint8_t CRSF::CSFR_RXpin_Module = GPIO_PIN_RCSIGNAL_RX; // Same pin for RX/TX

volatile bool CRSF::ignoreSerialData = false;
volatile bool CRSF::CRSFframeActive = false; //since we get a copy of the serial data use this flag to know when to ignore it

void inline CRSF::nullCallback(void) {}

void (*CRSF::RCdataCallback1)() = &nullCallback; // null placeholder callback
void (*CRSF::RCdataCallback2)() = &nullCallback; // null placeholder callback

void (*CRSF::disconnected)() = &nullCallback; // called when CRSF stream is lost
void (*CRSF::connected)() = &nullCallback;    // called when CRSF stream is regained

void (*CRSF::RecvParameterUpdate)() = &nullCallback; // called when recv parameter update req, ie from LUA

/// UART Handling ///
bool CRSF::CRSFstate = false;
uint32_t CRSF::UARTcurrentBaud = CRSF_OPENTX_FAST_BAUDRATE;
uint32_t CRSF::UARTrequestedBaud = CRSF_OPENTX_FAST_BAUDRATE;

uint32_t CRSF::lastUARTpktTime = 0;
uint32_t CRSF::UARTwdtLastChecked = 0;
uint32_t CRSF::UARTwdtInterval = 1000; // for the UART wdt, every 1000ms we change bauds when connect is lost

uint32_t CRSF::GoodPktsCount = 0;
uint32_t CRSF::BadPktsCount = 0;
uint32_t CRSF::GoodPktsCountResult = 0;
uint32_t CRSF::BadPktsCountResult = 0;
//luaxx
//uint32_t CRSF::FuncMode = 1;

volatile uint8_t CRSF::SerialInPacketLen = 0; // length of the CRSF packet as measured
volatile uint8_t CRSF::SerialInPacketPtr = 0; // index where we are reading/writing

volatile uint16_t CRSF::ChannelDataIn[16] = {0};
volatile uint16_t CRSF::ChannelDataInPrev[16] = {0};

volatile inBuffer_U CRSF::inBuffer;

// current and sent switch values, used for prioritising sequential switch transmission
uint8_t CRSF::currentSwitches[N_SWITCHES] = {0};
uint8_t CRSF::sentSwitches[N_SWITCHES] = {0};

uint8_t CRSF::nextSwitchIndex = 0; // for round-robin sequential switches

volatile uint8_t CRSF::ParameterUpdateData[2] = {0};

volatile crsf_channels_s CRSF::PackedRCdataOut;
volatile crsfPayloadLinkstatistics_s CRSF::LinkStatistics;
volatile crsf_sensor_battery_s CRSF::TLMbattSensor;

volatile uint32_t CRSF::RCdataLastRecv = 0;
volatile int32_t CRSF::OpenTXsyncOffset = 0;
#define SyncWaitPeriod 2000
uint32_t CRSF::SyncWaitPeriodCounter = 0;
LPF LPF_OPENTX_SYNC_OFFSET(3);
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
uint32_t CRSF::OpenTXsyncOffsetSafeMargin = 1000;
LPF LPF_OPENTX_SYNC_MARGIN(3);
#else
uint32_t CRSF::OpenTXsyncOffsetSafeMargin = 4000; // 400us
#endif
volatile uint32_t CRSF::OpenTXsyncLastSent = 0;
uint32_t CRSF::RequestedRCpacketInterval = 5000; // default to 200hz as per 'normal'

void CRSF::Begin()
{
    Serial.println("About to start CRSF task...");

#ifdef PLATFORM_ESP32
    mutexOutFIFO = xSemaphoreCreateMutex();
    xTaskCreatePinnedToCore(ESP32uartTask, "ESP32uartTask", 3000, NULL, 10, &xESP32uartTask, 1);
    xTaskCreatePinnedToCore(UARTwdt, "ESP32uartWDTTask", 2000, NULL, 10, &xESP32uartWDT, 1);
#endif
#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX) || defined(TARGET_R9M_LITE_PRO_TX)
    // TODO: Find out if xTaskCreate is a substitute for xTaskCreatePinnedToCore
    Serial.println("STM32 Platform Detected...");
    CRSF::STM32initUART();
#endif
    //The master module requires that the serial communication is bidirectional
    //The Reciever uses seperate rx and tx pins
}

void CRSF::End()
{
#ifdef PLATFORM_ESP32
    vTaskDelete(xESP32uartTask);
    vTaskDelete(xESP32uartWDT);
#endif
}

/**
 * Determine which switch to send next.
 * If any switch has changed since last sent, we send the lowest index changed switch
 * and set nextSwitchIndex to that value + 1.
 * If no switches have changed then we send nextSwitchIndex and increment the value.
 * For pure sequential switches, all 8 switches are part of the round-robin sequence.
 * For hybrid switches, switch 0 is sent with every packet and the rest of the switches
 * are in the round-robin.
 */
uint8_t ICACHE_RAM_ATTR CRSF::getNextSwitchIndex()
{
    int firstSwitch = 0; // sequential switches includes switch 0

#if defined(HYBRID_SWITCHES_8) || defined(ANALOG_7)
    firstSwitch = 1; // skip 0 since it is sent on every packet
#endif

    // look for a changed switch
    int i;
    for (i = firstSwitch; i < N_SWITCHES; i++)
    {
        if (currentSwitches[i] != sentSwitches[i])
            break;
    }
    // if we didn't find a changed switch, we get here with i==N_SWITCHES
    if (i == N_SWITCHES)
    {
        i = nextSwitchIndex;
    }

    // keep track of which switch to send next if there are no changed switches
    // during the next call.
    nextSwitchIndex = (i + 1) % 8;

#if defined(HYBRID_SWITCHES_8) || defined(ANALOG_7)
    // for hydrid switches 0 is sent on every packet, so we can skip
    // that value for the round-robin
    if (nextSwitchIndex == 0)
    {
        nextSwitchIndex = 1;
    }
#endif

    return i;
}

/**
 * Record the value of a switch that was sent to the rx
 */
void ICACHE_RAM_ATTR CRSF::setSentSwitch(uint8_t index, uint8_t value)
{
    sentSwitches[index] = value;
}

#if defined(PLATFORM_ESP32) || defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX) || defined(TARGET_R9M_LITE_PRO_TX)
void ICACHE_RAM_ATTR CRSF::sendLinkStatisticsToTX()
{
    uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = LinkStatisticsFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(outBuffer + 3, (byte *)&LinkStatistics, LinkStatisticsFrameLength);

    uint8_t crc = crsf_crc.calc(&outBuffer[2], LinkStatisticsFrameLength + 1);

    outBuffer[LinkStatisticsFrameLength + 3] = crc;

    if (CRSF::CRSFstate)
    {
#ifdef PLATFORM_ESP32
        portENTER_CRITICAL(&FIFOmux);
#endif
        SerialOutFIFO.push(LinkStatisticsFrameLength + 4); // length
        SerialOutFIFO.pushBytes(outBuffer, LinkStatisticsFrameLength + 4);
#ifdef PLATFORM_ESP32
        portEXIT_CRITICAL(&FIFOmux);
#endif
    }
}
//luaxx
void CRSF::sendLUAresponse(uint8_t val[], uint8_t len)
{
    uint8_t LUArespLength = len + 2;
    uint8_t outBuffer[LUArespLength + 5] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = LUArespLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_PARAMETER_WRITE;

    outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[4] = CRSF_ADDRESS_CRSF_TRANSMITTER;

    for (uint8_t i = 0; i < len; ++i)
    {
        outBuffer[5 + i] = val[i];
    }

    uint8_t crc = crsf_crc.calc(&outBuffer[2], LUArespLength + 1);

    outBuffer[LUArespLength + 3] = crc;

    if (CRSF::CRSFstate)
    {
#ifdef PLATFORM_ESP32
        portENTER_CRITICAL(&FIFOmux);
#endif
        SerialOutFIFO.push(LUArespLength + 4); // length
        SerialOutFIFO.pushBytes(outBuffer, LUArespLength + 4);
#ifdef PLATFORM_ESP32
        portEXIT_CRITICAL(&FIFOmux);
#endif
    }
}

void ICACHE_RAM_ATTR CRSF::sendLinkBattSensorToTX()
{
    uint8_t outBuffer[BattSensorFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = BattSensorFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_BATTERY_SENSOR;

    outBuffer[3] = CRSF::TLMbattSensor.voltage << 8;
    outBuffer[4] = CRSF::TLMbattSensor.voltage;

    outBuffer[5] = CRSF::TLMbattSensor.current << 8;
    outBuffer[6] = CRSF::TLMbattSensor.current;

    outBuffer[7] = CRSF::TLMbattSensor.capacity << 16;
    outBuffer[9] = CRSF::TLMbattSensor.capacity << 8;

    outBuffer[10] = CRSF::TLMbattSensor.capacity;
    outBuffer[11] = CRSF::TLMbattSensor.remaining;

    uint8_t crc = crsf_crc.calc(&outBuffer[2], BattSensorFrameLength + 1);

    outBuffer[BattSensorFrameLength + 3] = crc;

    if (CRSF::CRSFstate)
    {
#ifdef PLATFORM_ESP32
        xSemaphoreTake(mutexOutFIFO, portMAX_DELAY);
#endif
        SerialOutFIFO.push(BattSensorFrameLength + 4); // length
        SerialOutFIFO.pushBytes(outBuffer, BattSensorFrameLength + 4);
#ifdef PLATFORM_ESP32
        xSemaphoreGive(mutexOutFIFO);
#endif
    }
}

void ICACHE_RAM_ATTR CRSF::setSyncParams(uint32_t PacketInterval)
{
    CRSF::RequestedRCpacketInterval = PacketInterval;
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
    CRSF::SyncWaitPeriodCounter = millis();
    CRSF::OpenTXsyncOffsetSafeMargin = 1000;
    LPF_OPENTX_SYNC_MARGIN.init(0);
    LPF_OPENTX_SYNC_OFFSET.init(0);
#endif
}

void ICACHE_RAM_ATTR CRSF::JustSentRFpacket()
{
    CRSF::OpenTXsyncOffset = micros() - CRSF::RCdataLastRecv;

    if (CRSF::OpenTXsyncOffset > (int32_t)CRSF::RequestedRCpacketInterval) // detect overrun case when the packet arrives too late and caculate negative offsets.
    {
        CRSF::OpenTXsyncOffset = -(CRSF::OpenTXsyncOffset % CRSF::RequestedRCpacketInterval);
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
        if (millis() > CRSF::SyncWaitPeriodCounter + SyncWaitPeriod) // wait until we stablize after changing pkt rate
        {
            CRSF::OpenTXsyncOffsetSafeMargin = LPF_OPENTX_SYNC_MARGIN.update((CRSF::OpenTXsyncOffsetSafeMargin - OpenTXsyncOffset) + 100); // take worst case plus 50us
        }
#endif
    }

#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
    if (CRSF::OpenTXsyncOffsetSafeMargin > 4000)
    {
        CRSF::OpenTXsyncOffsetSafeMargin = 4000; // hard limit at no tune default
    }
    else if (CRSF::OpenTXsyncOffsetSafeMargin < 1000)
    {
        CRSF::OpenTXsyncOffsetSafeMargin = 1000; // hard limit at no tune default
    }
#endif
    // Serial.print(CRSF::OpenTXsyncOffset);
    // Serial.print(",");
    // Serial.println(CRSF::OpenTXsyncOffsetSafeMargin / 10);
}

#ifdef PLATFORM_ESP32
void ICACHE_RAM_ATTR CRSF::sendSyncPacketToTX(void *pvParameters) // in values in us.
{
    //vTaskDelay(500);
    for (;;)
    {
#endif

#ifdef PLATFORM_STM32
        void ICACHE_RAM_ATTR CRSF::sendSyncPacketToTX() // in values in us.
        {
            if (millis() > OpenTXsyncLastSent + OpenTXsyncPacketInterval)
            {
#endif
                uint32_t packetRate = CRSF::RequestedRCpacketInterval * 10; //convert from us to right format
                int32_t offset = CRSF::OpenTXsyncOffset * 10 - CRSF::OpenTXsyncOffsetSafeMargin; // + 400us offset that that opentx always has some headroom

                uint8_t outBuffer[OpenTXsyncFrameLength + 4] = {0};

                outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER; //0xEA
                outBuffer[1] = OpenTXsyncFrameLength + 2;      // equals 13?
                outBuffer[2] = CRSF_FRAMETYPE_RADIO_ID;        // 0x3A

                outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER; //0XEA
                outBuffer[4] = 0x00;                           //??? not sure doesn't seem to matter
                outBuffer[5] = CRSF_FRAMETYPE_OPENTX_SYNC;     //0X10

                outBuffer[6] = (packetRate & 0xFF000000) >> 24;
                outBuffer[7] = (packetRate & 0x00FF0000) >> 16;
                outBuffer[8] = (packetRate & 0x0000FF00) >> 8;
                outBuffer[9] = (packetRate & 0x000000FF) >> 0;

                outBuffer[10] = (offset & 0xFF000000) >> 24;
                outBuffer[11] = (offset & 0x00FF0000) >> 16;
                outBuffer[12] = (offset & 0x0000FF00) >> 8;
                outBuffer[13] = (offset & 0x000000FF) >> 0;

                uint8_t crc = crsf_crc.calc(&outBuffer[2], OpenTXsyncFrameLength + 1);

                outBuffer[OpenTXsyncFrameLength + 3] = crc;

                if (CRSF::CRSFstate)
                {
#ifdef PLATFORM_ESP32
                    portENTER_CRITICAL(&FIFOmux);
#endif
                    SerialOutFIFO.push(OpenTXsyncFrameLength + 4); // length
                    SerialOutFIFO.pushBytes(outBuffer, OpenTXsyncFrameLength + 4);
#ifdef PLATFORM_ESP32
                    portEXIT_CRITICAL(&FIFOmux);
#endif
                }
                OpenTXsyncLastSent = millis();
#ifdef PLATFORM_ESP32
                vTaskDelay(OpenTXsyncPacketInterval);
#endif
            }
        }

#endif

#if defined(PLATFORM_ESP8266) || defined(TARGET_R9M_RX) || defined(TARGET_RX_GHOST_ATTO_V1) || defined(TARGET_SX1280_RX_CCG_NANO_v05) ||defined(UNIT_TEST)
        bool CRSF::RXhandleUARTout()
        {
            uint8_t peekVal = SerialOutFIFO.peek(); // check if we have data in the output FIFO that needs to be written
            if (peekVal > 0)
            {
                if (SerialOutFIFO.size() > (peekVal))
                {
                    noInterrupts();
                    uint8_t OutPktLen = SerialOutFIFO.pop();
                    uint8_t OutData[OutPktLen];
                    SerialOutFIFO.popBytes(OutData, OutPktLen);
                    interrupts();
                    this->_dev->write(OutData, OutPktLen); // write the packet out
                    return true;
                }
            }
            return false;
        }

        void ICACHE_RAM_ATTR CRSF::sendLinkStatisticsToFC()
        {
            uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

            outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
            outBuffer[1] = LinkStatisticsFrameLength + 2;
            outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

            memcpy(outBuffer + 3, (byte *)&LinkStatistics, LinkStatisticsFrameLength);

            uint8_t crc = crsf_crc.calc(&outBuffer[2], LinkStatisticsFrameLength + 1);

            outBuffer[LinkStatisticsFrameLength + 3] = crc;
#ifndef DEBUG_CRSF_NO_OUTPUT
            SerialOutFIFO.push(LinkStatisticsFrameLength + 4);
            SerialOutFIFO.pushBytes(outBuffer, LinkStatisticsFrameLength + 4);
            //this->_dev->write(outBuffer, LinkStatisticsFrameLength + 4);
#endif
        }

        void ICACHE_RAM_ATTR CRSF::sendRCFrameToFC()
        {
            uint8_t outBuffer[RCframeLength + 4] = {0};

            outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
            outBuffer[1] = RCframeLength + 2;
            outBuffer[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

            memcpy(outBuffer + 3, (byte *)&PackedRCdataOut, RCframeLength);

            uint8_t crc = crsf_crc.calc(&outBuffer[2], RCframeLength + 1);

            outBuffer[RCframeLength + 3] = crc;
#ifndef DEBUG_CRSF_NO_OUTPUT
            //SerialOutFIFO.push(RCframeLength + 4);
            //SerialOutFIFO.pushBytes(outBuffer, RCframeLength + 4);
            this->_dev->write(outBuffer, RCframeLength + 4);
#endif
        }

        void ICACHE_RAM_ATTR CRSF::sendMSPFrameToFC(mspPacket_t * packet)
        {
            // TODO: This currently only supports single MSP packets per cmd
            // To support longer packets we need to re-write this to allow packet splitting
            const uint8_t totalBufferLen = ENCAPSULATED_MSP_FRAME_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + CRSF_FRAME_NOT_COUNTED_BYTES;
            uint8_t outBuffer[totalBufferLen] = {0};

            // CRSF extended frame header
            outBuffer[0] = CRSF_ADDRESS_BROADCAST;                                      // address
            outBuffer[1] = ENCAPSULATED_MSP_FRAME_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC; // length
            outBuffer[2] = CRSF_FRAMETYPE_MSP_WRITE;                                    // packet type
            outBuffer[3] = CRSF_ADDRESS_FLIGHT_CONTROLLER;                              // destination
            outBuffer[4] = CRSF_ADDRESS_RADIO_TRANSMITTER;                              // origin

            // Encapsulated MSP payload
            outBuffer[5] = 0x30;                // header
            outBuffer[6] = packet->payloadSize; // mspPayloadSize
            outBuffer[7] = packet->function;    // packet->cmd
            for (uint8_t i = 0; i < ENCAPSULATED_MSP_PAYLOAD_SIZE; ++i)
            {
                // copy packet payload into outBuffer and pad with zeros where required
                outBuffer[8 + i] = i < packet->payloadSize ? packet->payload[i] : 0;
            }
            // Encapsulated MSP crc
            outBuffer[totalBufferLen - 2] = CalcCRCMsp(&outBuffer[6], ENCAPSULATED_MSP_FRAME_LEN - 2);

            // CRSF frame crc
            outBuffer[totalBufferLen - 1] = crsf_crc.calc(&outBuffer[2], ENCAPSULATED_MSP_FRAME_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC - 1);

            // SerialOutFIFO.push(totalBufferLen);
            // SerialOutFIFO.pushBytes(outBuffer, totalBufferLen);
            this->_dev->write(outBuffer, totalBufferLen);
        }
#endif

#if defined(PLATFORM_ESP32) || defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX) || defined(TARGET_R9M_LITE_PRO_TX)

#ifdef PLATFORM_ESP32
    void ICACHE_RAM_ATTR CRSF::UARTwdt(void *pvParameters) // in values in us.
    {
        vTaskDelay(UARTwdtInterval); // adds a small delay so that the WDT function doesn't trigger immediately at boot
        for (;;)
        {
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX) || defined(TARGET_R9M_LITE_PRO_TX)
            void  CRSF::UARTwdt()
            {
                if (millis() > (UARTwdtLastChecked + UARTwdtInterval))
                {
#endif
                    if (BadPktsCount >= GoodPktsCount)
                    {
                        Serial.print("Too many bad UART RX packets! ");

                        if (CRSFstate == true)
                        {
                            Serial.println("CRSF UART Disconnected");
                            SyncWaitPeriodCounter = millis(); // set to begin wait for auto sync offset calculation
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
                            CRSF::OpenTXsyncOffsetSafeMargin = 1000;
                            CRSF::OpenTXsyncOffset = 0;
                            CRSF::OpenTXsyncLastSent = 0;
#endif
                            disconnected();
                            CRSFstate = false;
                        }

                        if (UARTcurrentBaud == CRSF_OPENTX_FAST_BAUDRATE)
                        {
                            UARTrequestedBaud = CRSF_OPENTX_SLOW_BAUDRATE;
                        }
                        else if (UARTcurrentBaud == CRSF_OPENTX_SLOW_BAUDRATE)
                        {
                            UARTrequestedBaud = CRSF_OPENTX_FAST_BAUDRATE;
                        }

                        Serial.print("UART WDT: Switch to: ");
                        Serial.print(UARTrequestedBaud);
                        Serial.println(" baud");
                    }
                    Serial.print("UART STATS Bad:Good = ");
                    Serial.print(BadPktsCount);
                    Serial.print(":");
                    Serial.println(GoodPktsCount);

                    UARTwdtLastChecked = millis();
                    GoodPktsCountResult = GoodPktsCount;
                    BadPktsCountResult = BadPktsCount;
                    BadPktsCount = 0;
                    GoodPktsCount = 0;

#ifdef PLATFORM_ESP32
                    vTaskDelay(UARTwdtInterval);
#endif
                }
            }
#endif

#ifdef PLATFORM_ESP32

            void ICACHE_RAM_ATTR CRSF::duplex_set_RX()
            {
                ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_MODE_INPUT));
                gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, true);
                #ifdef UART_INVERTED
                gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, true);
                gpio_pulldown_en((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
                gpio_pullup_dis((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
                #else
                gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, false);
                gpio_pullup_en((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
                gpio_pulldown_dis((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
                #endif
            }
            void ICACHE_RAM_ATTR CRSF::duplex_set_TX()
            {
                gpio_matrix_in((gpio_num_t)-1, U1RXD_IN_IDX, false);
                ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_FLOATING));
                ESP_ERROR_CHECK(gpio_set_pull_mode((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_FLOATING));
                ESP_ERROR_CHECK(gpio_set_level((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, 0));
                ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, GPIO_MODE_OUTPUT));
                #ifdef UART_INVERTED
                gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U1TXD_OUT_IDX, true, false);
                #else
                gpio_matrix_out((gpio_num_t)GPIO_PIN_RCSIGNAL_TX, U1TXD_OUT_IDX, false, false);
                #endif
            }

            void ICACHE_RAM_ATTR CRSF::ESP32uartTask(void *pvParameters) //RTOS task to read and write CRSF packets to the serial port
            {
                CRSF::Port.begin(CRSF_OPENTX_FAST_BAUDRATE, SERIAL_8N1, CSFR_RXpin_Module, CSFR_TXpin_Module, false, 500);
                UARTcurrentBaud = CRSF_OPENTX_FAST_BAUDRATE;
                // const TickType_t xDelay1 = 1 / portTICK_PERIOD_MS;
                Serial.println("ESP32 CRSF UART LISTEN TASK STARTED");
                CRSF::duplex_set_RX();

                while (CRSF::Port.available())
                {
                    CRSF::Port.read(); // measure sure there is no garbage on the UART at the start
                }

                vTaskDelay(5); // wait for the first packet to arrive

                volatile uint8_t *SerialInBuffer = CRSF::inBuffer.asUint8_t;

                for (;;)
                {
                    if (CRSF::Port.available())
                    {
                        char inChar = CRSF::Port.read();

                        if (CRSFframeActive == false)
                        {
                            // stage 1 wait for sync byte //
                            if (inChar == CRSF_ADDRESS_CRSF_TRANSMITTER || inChar == CRSF_SYNC_BYTE) // we got sync, reset write pointer
                            {
                                SerialInPacketPtr = 0;
                                SerialInPacketLen = 0;
                                CRSFframeActive = true;
                                SerialInBuffer[SerialInPacketPtr] = inChar;
                                SerialInPacketPtr++;
                            }
                        }
                        else // frame is active so we do the processing
                        {
                            // first if things have gone wrong //
                            if (SerialInPacketPtr > CRSF_MAX_PACKET_LEN - 1) // we reached the maximum allowable packet length, so start again because shit fucked up hey.
                            {
                                SerialInPacketPtr = 0;
                                SerialInPacketLen = 0;
                                CRSFframeActive = false;
                                continue;
                            }

                            // special case where we save the expected pkt len to buffer //
                            if (SerialInPacketPtr == 1)
                            {
                                if (inChar <= CRSF_MAX_PACKET_LEN)
                                {
                                    SerialInPacketLen = inChar;
                                }
                                else
                                {
                                    SerialInPacketPtr = 0;
                                    SerialInPacketLen = 0;
                                    CRSFframeActive = false;
                                    continue;
                                }
                            }

                            SerialInBuffer[SerialInPacketPtr] = inChar;
                            SerialInPacketPtr++;

                            if (SerialInPacketPtr >= (SerialInPacketLen + 2)) // plus 2 because the packlen is referenced from the start of the 'type' flag, IE there are an extra 2 bytes.
                            {
                                char CalculatedCRC = crsf_crc.calc((uint8_t *)SerialInBuffer + 2, SerialInPacketPtr - 3);

                                if (CalculatedCRC == inChar)
                                {
                                    GoodPktsCount++;
                                    ESP32ProcessPacket();
                                    lastUARTpktTime = millis();

                                    uint8_t peekVal = SerialOutFIFO.peek(); // check if we have data in the output FIFO that needs to be written
                                    if (peekVal > 0)
                                    {
                                        if (SerialOutFIFO.size() >= peekVal)
                                        {
                                            CRSF::duplex_set_TX();
                                            portENTER_CRITICAL(&FIFOmux); // stops other tasks from writing to the FIFO when we want to read it
                                            uint8_t OutPktLen = SerialOutFIFO.pop();
                                            uint8_t OutData[OutPktLen];
                                            SerialOutFIFO.popBytes(OutData, OutPktLen);
                                            portEXIT_CRITICAL(&FIFOmux);
                                            CRSF::Port.write(OutData, OutPktLen); // write the packet out
                                            CRSF::Port.flush();                   // flush makes sure all bytes are pushed.
                                            CRSF::duplex_set_RX();
                                            vTaskDelay(1); // we don't expect anything for while so feel free to delay
                                        }
                                    }
                                }
                                else
                                {
                                    BadPktsCount++;
                                    Serial.println("UART CRC failure");
                                }
                                CRSFframeActive = false;
                                SerialInPacketPtr = 0;
                                SerialInPacketLen = 0;
                            }
                        }
                    }
                    else
                    {
                        if (CRSFstate == false)
                        {
                            if (UARTrequestedBaud != UARTcurrentBaud) // watchdog request to change baudrate
                            {
                                SerialOutFIFO.flush();
                                CRSF::Port.flush();
                                CRSF::Port.updateBaudRate(UARTrequestedBaud);
                                UARTcurrentBaud = UARTrequestedBaud;
                                CRSF::duplex_set_RX();
                            }
                            vTaskDelay(1); //delay when crsf state is not connected, this will prevent WDT triggering
                        }
                    }
                }
            }
#endif

#ifdef PLATFORM_ESP32
            void ICACHE_RAM_ATTR CRSF::ESP32ProcessPacket()
            {
                if (CRSFstate == false)
                {
                    CRSFstate = true;
                    Serial.println("CRSF UART Connected");
#ifdef FEATURE_OPENTX_SYNC
                    xTaskCreatePinnedToCore(sendSyncPacketToTX, "sendSyncPacketToTX", 2000, NULL, 10, &xHandleOpenTXsync, 1);
#endif
                    SyncWaitPeriodCounter = millis(); // set to begin wait for auto sync offset calculation
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
                    LPF_OPENTX_SYNC_MARGIN.init(0);
                    LPF_OPENTX_SYNC_OFFSET.init(0);
#endif
                    connected();
                }

                const uint8_t packetType = CRSF::inBuffer.asRCPacket_t.header.type;

                if (packetType == CRSF_FRAMETYPE_PARAMETER_WRITE)
                {
                    const volatile uint8_t *buffer = CRSF::inBuffer.asUint8_t;
                    if (buffer[3] == CRSF_ADDRESS_CRSF_TRANSMITTER && buffer[4] == CRSF_ADDRESS_RADIO_TRANSMITTER)
                    {
                        ParameterUpdateData[0] = buffer[5];
                        ParameterUpdateData[1] = buffer[6];
                        RecvParameterUpdate();
                    }
                    Serial.println("Got Other Packet"); // TODO use debug macro?
                }

                if (packetType == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
                {
                    CRSF::RCdataLastRecv = micros();
                    GetChannelDataIn();
                    (RCdataCallback1)(); // run new RC data callback
                    (RCdataCallback2)(); // run new RC data callback
                }
            }
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX) || defined(TARGET_R9M_LITE_PRO_TX)

            void ICACHE_RAM_ATTR CRSF::STM32initUART() //RTOS task to read and write CRSF packets to the serial port
            {
                Serial.println("Start STM32 R9M TX CRSF UART");

                pinMode(BUFFER_OE, OUTPUT);

                CRSF::Port.setTx(GPIO_PIN_RCSIGNAL_TX);
                CRSF::Port.setRx(GPIO_PIN_RCSIGNAL_RX);
                CRSF::Port.begin(CRSF_OPENTX_FAST_BAUDRATE);
                UARTwdtLastChecked = millis() + UARTwdtInterval; // allows a delay before the first time the UARTwdt() function is called

                Serial.println("STM32 CRSF UART LISTEN TASK STARTED");
                CRSF::Port.flush();
            }

            void ICACHE_RAM_ATTR CRSF::STM32handleUARTin() //RTOS task to read and write CRSF packets to the serial port
            {
                volatile uint8_t *SerialInBuffer = CRSF::inBuffer.asUint8_t;

                if (UARTrequestedBaud != UARTcurrentBaud)
                {
                    CRSF::Port.begin(UARTrequestedBaud);
                    UARTcurrentBaud = UARTrequestedBaud;
                }

                while (CRSF::Port.available())
                {
                    char inChar = CRSF::Port.read();

                    if (CRSFframeActive == false)
                    {
                        // stage 1 wait for sync byte //
                        if (inChar == CRSF_ADDRESS_CRSF_TRANSMITTER || inChar == CRSF_SYNC_BYTE) // we got sync, reset write pointer
                        {
                            SerialInPacketPtr = 0;
                            SerialInPacketLen = 0;
                            CRSFframeActive = true;
                            SerialInBuffer[SerialInPacketPtr] = inChar;
                            SerialInPacketPtr++;
                        }
                    }
                    else // frame is active so we do the processing
                    {
                        // first if things have gone wrong //
                        if (SerialInPacketPtr > CRSF_MAX_PACKET_LEN - 1) // we reached the maximum allowable packet length, so start again because shit fucked up hey.
                        {
                            SerialInPacketPtr = 0;
                            SerialInPacketLen = 0;
                            CRSFframeActive = false;
                            return;
                        }

                        // special case where we save the expected pkt len to buffer //
                        if (SerialInPacketPtr == 1)
                        {
                            if (inChar <= CRSF_MAX_PACKET_LEN)
                            {
                                SerialInPacketLen = inChar;
                            }
                            else
                            {
                                SerialInPacketPtr = 0;
                                SerialInPacketLen = 0;
                                CRSFframeActive = false;
                                return;
                            }
                        }

                        SerialInBuffer[SerialInPacketPtr] = inChar;
                        SerialInPacketPtr++;

                        if (SerialInPacketPtr >= (SerialInPacketLen + 2)) // plus 2 because the packlen is referenced from the start of the 'type' flag, IE there are an extra 2 bytes.
                        {
                            char CalculatedCRC = crsf_crc.calc((uint8_t *)SerialInBuffer + 2, SerialInPacketPtr - 3);

                            if (CalculatedCRC == inChar)
                            {
                                if (STM32ProcessPacket())
                                {
                                    lastUARTpktTime = millis();
                                    delayMicroseconds(50);
                                    CRSF::STM32handleUARTout();
                                    GoodPktsCount++;
                                }
                            }
                            else
                            {
                                Serial.println("UART CRC failure");
                                CRSF::Port.flush();
                                BadPktsCount++;
                            }
                            CRSFframeActive = false;
                            SerialInPacketPtr = 0;
                            SerialInPacketLen = 0;
                        }
                    }
                }
            }

            void ICACHE_RAM_ATTR CRSF::STM32handleUARTout()
            {
                uint8_t peekVal = SerialOutFIFO.peek(); // check if we have data in the output FIFO that needs to be written
                if (peekVal > 0)
                {
                    if (SerialOutFIFO.size() >= (peekVal + 1))
                    {
                        digitalWrite(BUFFER_OE, HIGH);

                        uint8_t OutPktLen = SerialOutFIFO.pop();
                        uint8_t OutData[OutPktLen];

                        SerialOutFIFO.popBytes(OutData, OutPktLen);
                        CRSF::Port.write(OutData, OutPktLen); // write the packet out
                        CRSF::Port.flush();
                        digitalWrite(BUFFER_OE, LOW);
                        while (CRSF::Port.available())
                        {
                            CRSF::Port.read(); // measure sure there is no garbage on the UART at the start
                        }
                    }
                }
            }

            bool ICACHE_RAM_ATTR CRSF::STM32ProcessPacket()
            {
                if (CRSFstate == false)
                {
                    CRSFstate = true;
                    Serial.println("CRSF UART Connected");
                    SyncWaitPeriodCounter = millis(); // set to begin wait for auto sync offset calculation
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
                    LPF_OPENTX_SYNC_MARGIN.init(0);
                    LPF_OPENTX_SYNC_OFFSET.init(0);
#endif
                    connected();
                }

                const uint8_t packetType = CRSF::inBuffer.asRCPacket_t.header.type;

                if (packetType == CRSF_FRAMETYPE_PARAMETER_WRITE)
                {
                    const volatile uint8_t *SerialInBuffer = CRSF::inBuffer.asUint8_t;
                    if (SerialInBuffer[3] == CRSF_ADDRESS_CRSF_TRANSMITTER && SerialInBuffer[4] == CRSF_ADDRESS_RADIO_TRANSMITTER)
                    {
                        ParameterUpdateData[0] = SerialInBuffer[5];
                        ParameterUpdateData[1] = SerialInBuffer[6];
                        RecvParameterUpdate();
                        return true;
                    }
                    Serial.println("Got Other Packet");
                }

                if (packetType == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
                {
                    CRSF::RCdataLastRecv = micros();
                    GetChannelDataIn();
                    (RCdataCallback1)(); // run new RC data callback
                    (RCdataCallback2)(); // run new RC data callback
                    return true;
                }
                return false;
            }

#endif

            /**
 * Convert the rc data corresponding to switches to 2 bit values.
 *
 * I'm defining channels 4 through 11 inclusive as representing switches
 * Take the input values and convert them to the range 0 - 2.
 * (not 0-3 because most people use 3 way switches and expect the middle
 *  position to be represented by a middle numeric value)
 */
            void ICACHE_RAM_ATTR CRSF::updateSwitchValues()
            {
#define INPUT_RANGE 2048
                const uint16_t SWITCH_DIVISOR = INPUT_RANGE / 3; // input is 0 - 2048
                //if(FuncMode = 0){
                    for (int i = 0; i < N_SWITCHES; i++)
                  {
                      if(i==0){
                        currentSwitches[i] = ChannelDataIn[4] / SWITCH_DIVISOR;    
                      } else {
                        currentSwitches[i] = ChannelDataIn[i + 7] / SWITCH_DIVISOR;
                      }
                  }
                //} else {
                //for (int i = 0; i < N_SWITCHES; i++)
                //{
                //    currentSwitches[i] = ChannelDataIn[i + 4] / SWITCH_DIVISOR;
                //}
                //luaxx
                //}
            }

            void ICACHE_RAM_ATTR CRSF::GetChannelDataIn() // data is packed as 11 bits per channel
            {
#define SERIAL_PACKET_OFFSET 3

                memcpy((uint16_t *)ChannelDataInPrev, (uint16_t *)ChannelDataIn, 16); //before we write the new RC channel data copy the old data

                const volatile crsf_channels_t *rcChannels = &CRSF::inBuffer.asRCPacket_t.channels;
                ChannelDataIn[0] = (rcChannels->ch0);
                ChannelDataIn[1] = (rcChannels->ch1);
                ChannelDataIn[2] = (rcChannels->ch2);
                ChannelDataIn[3] = (rcChannels->ch3);
                ChannelDataIn[4] = (rcChannels->ch4);
                ChannelDataIn[5] = (rcChannels->ch5);
                ChannelDataIn[6] = (rcChannels->ch6);
                ChannelDataIn[7] = (rcChannels->ch7);
                ChannelDataIn[8] = (rcChannels->ch8);
                ChannelDataIn[9] = (rcChannels->ch9);
                ChannelDataIn[10] = (rcChannels->ch10);
                ChannelDataIn[11] = (rcChannels->ch11);
                ChannelDataIn[12] = (rcChannels->ch12);
                ChannelDataIn[13] = (rcChannels->ch13);
                ChannelDataIn[14] = (rcChannels->ch14);
                ChannelDataIn[15] = (rcChannels->ch15);

                updateSwitchValues();
            }

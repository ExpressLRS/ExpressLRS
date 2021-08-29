#include "CRSF.h"
#include "../../lib/FIFO/FIFO.h"
#include "telemetry_protocol.h"
#include "logging.h"

#ifdef PLATFORM_ESP32
HardwareSerial SerialPort(1);
HardwareSerial CRSF::Port = SerialPort;
portMUX_TYPE FIFOmux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t xHandleOpenTXsync = NULL;
TaskHandle_t xESP32uartTask = NULL;
#elif CRSF_TX_MODULE_STM32
HardwareSerial CRSF::Port(GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX);
#if defined(STM32F3) || defined(STM32F3xx)
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_gpio.h"
#elif defined(STM32F1) || defined(STM32F1xx)
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#endif
#endif

#define CHUNK_MAX_NUMBER_OF_BYTES 16 //safe number of bytes to not get intterupted between rc packets
GENERIC_CRC8 crsf_crc(CRSF_CRC_POLY);


///Out FIFO to buffer messages///
FIFO SerialOutFIFO;
FIFO MspWriteFIFO;

volatile bool CRSF::CRSFframeActive = false; //since we get a copy of the serial data use this flag to know when to ignore it

void inline CRSF::nullCallback(void) {}

void (*CRSF::disconnected)() = &nullCallback; // called when CRSF stream is lost
void (*CRSF::connected)() = &nullCallback;    // called when CRSF stream is regained

void (*CRSF::RecvParameterUpdate)() = &nullCallback; // called when recv parameter update req, ie from LUA
void (*CRSF::RecvModelUpdate)() = &nullCallback; // called when model id cahnges, ie command from Radio
void (*CRSF::RCdataCallback)() = &nullCallback; // called when there is new RC data

/// UART Handling ///
uint32_t CRSF::GoodPktsCountResult = 0;
uint32_t CRSF::BadPktsCountResult = 0;

bool CRSF::hasEverConnected = false;

volatile uint8_t CRSF::SerialInPacketLen = 0; // length of the CRSF packet as measured
volatile uint8_t CRSF::SerialInPacketPtr = 0; // index where we are reading/writing

volatile uint16_t CRSF::ChannelDataIn[16] = {0};

volatile inBuffer_U CRSF::inBuffer;

uint8_t CRSF::modelId = 0;

volatile uint8_t CRSF::ParameterUpdateData[3] = {0};
volatile bool CRSF::elrsLUAmode = false;

volatile crsf_channels_s CRSF::PackedRCdataOut;
volatile crsfPayloadLinkstatistics_s CRSF::LinkStatistics;
volatile crsf_sensor_battery_s CRSF::TLMbattSensor;

#if CRSF_TX_MODULE
/// OpenTX mixer sync ///
volatile uint32_t CRSF::OpenTXsyncLastSent = 0;
uint32_t CRSF::RequestedRCpacketInterval = 5000; // default to 200hz as per 'normal'
volatile uint32_t CRSF::RCdataLastRecv = 0;
volatile int32_t CRSF::OpenTXsyncOffset = 0;
bool CRSF::OpentxSyncActive = true;

#define MAX_BYTES_SENT_IN_UART_OUT 32
uint8_t CRSF::CRSFoutBuffer[CRSF_MAX_PACKET_LEN] = {0};

#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
#define AutoSyncWaitPeriod 2000
uint32_t CRSF::OpenTXsyncOffsetSafeMargin = 1000;
static LPF LPF_OPENTX_SYNC_MARGIN(3);
static LPF LPF_OPENTX_SYNC_OFFSET(3);
uint32_t CRSF::SyncWaitPeriodCounter = 0;
#else
uint32_t CRSF::OpenTXsyncOffsetSafeMargin = 4000; // 400us
#endif

/// UART Handling ///
uint32_t CRSF::GoodPktsCount = 0;
uint32_t CRSF::BadPktsCount = 0;
uint32_t CRSF::UARTwdtLastChecked;
uint32_t CRSF::UARTcurrentBaud;
bool CRSF::CRSFstate = false;

// for the UART wdt, every 1000ms we change bauds when connect is lost
#define UARTwdtInterval 1000

uint8_t CRSF::MspData[ELRS_MSP_BUFFER] = {0};
uint8_t CRSF::MspDataLength = 0;

uint32_t CRSF::luaHiddenFlags = 0;
#endif // CRSF_TX_MODULE


void CRSF::Begin()
{
    DBGLN("About to start CRSF task...");

#if CRSF_TX_MODULE
    UARTcurrentBaud = CRSF_OPENTX_FAST_BAUDRATE;
    UARTwdtLastChecked = millis() + UARTwdtInterval; // allows a delay before the first time the UARTwdt() function is called

#ifdef PLATFORM_ESP32
    disableCore0WDT();
    xTaskCreatePinnedToCore(ESP32uartTask, "ESP32uartTask", 3000, NULL, 0, &xESP32uartTask, 0);


#elif defined(PLATFORM_STM32)
    DBGLN("Start STM32 R9M TX CRSF UART");

    CRSF::Port.setTx(GPIO_PIN_RCSIGNAL_TX);
    CRSF::Port.setRx(GPIO_PIN_RCSIGNAL_RX);

    #if defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    pinMode(GPIO_PIN_BUFFER_OE, OUTPUT);
    digitalWrite(GPIO_PIN_BUFFER_OE, LOW ^ GPIO_PIN_BUFFER_OE_INVERTED); // RX mode default
    #elif (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    CRSF::Port.setHalfDuplex();
    #endif

    CRSF::Port.begin(CRSF_OPENTX_FAST_BAUDRATE);

#if defined(TARGET_TX_GHOST)
    USART1->CR1 &= ~USART_CR1_UE;
    USART1->CR3 |= USART_CR3_HDSEL;
    USART1->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV | USART_CR2_SWAP; //inverted/swapped
    USART1->CR1 |= USART_CR1_UE;
#endif
#if defined(TARGET_TX_FM30_MINI)
    LL_GPIO_SetPinPull(GPIOA, GPIO_PIN_2, LL_GPIO_PULL_DOWN); // default is PULLUP
    USART2->CR1 &= ~USART_CR1_UE;
    USART2->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV; //inverted
    USART2->CR1 |= USART_CR1_UE;
#endif
    DBGLN("STM32 CRSF UART LISTEN TASK STARTED");
    CRSF::Port.flush();
#endif

    flush_port_input();
#endif // CRSF_TX_MODULE

    //The master module requires that the serial communication is bidirectional
    //The Reciever uses seperate rx and tx pins
}

void CRSF::End()
{
#if CRSF_TX_MODULE
#ifdef PLATFORM_ESP32
    if (xESP32uartTask != NULL)
    {
        vTaskDelete(xESP32uartTask);
    }
#endif
    uint32_t startTime = millis();
    #define timeout 2000
    while (SerialOutFIFO.peek() > 0)
    {
        handleUARTin();
        if (millis() - startTime > 1000)
        {
            break;
        }
    }
    //CRSF::Port.end(); // don't call seria.end(), it causes some sort of issue with the 900mhz hardware using gpio2 for serial 
    DBGLN("CRSF UART END");
#endif // CRSF_TX_MODULE
}

void CRSF::flush_port_input(void)
{
    // Make sure there is no garbage on the UART at the start
    while (CRSF::Port.available())
    {
        CRSF::Port.read();
    }
}

#if CRSF_TX_MODULE
void ICACHE_RAM_ATTR CRSF::sendLinkStatisticsToTX()
{
    if (!CRSF::CRSFstate)
    {
        return;
    }

    uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    outBuffer[1] = LinkStatisticsFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(outBuffer + 3, (byte *)&LinkStatistics, LinkStatisticsFrameLength);

    uint8_t crc = crsf_crc.calc(&outBuffer[2], LinkStatisticsFrameLength + 1);

    outBuffer[LinkStatisticsFrameLength + 3] = crc;

#ifdef PLATFORM_ESP32
    portENTER_CRITICAL(&FIFOmux);
#endif
    SerialOutFIFO.push(LinkStatisticsFrameLength + 4); // length
    SerialOutFIFO.pushBytes(outBuffer, LinkStatisticsFrameLength + 4);
#ifdef PLATFORM_ESP32
    portEXIT_CRITICAL(&FIFOmux);
#endif
}

/**
 * Build a an extended type packet and queue it in the SerialOutFIFO
 * This is just a regular packet with 2 extra bytes with the sub src and target
 **/
void CRSF::packetQueueExtended(uint8_t type, void *data, uint8_t len)
{
    if (!CRSF::CRSFstate)
        return;

    uint8_t buf[6 + len];
    // Header info
    buf[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    buf[1] = len + 4; // Type + DST + SRC + CRC
    buf[2] = type;
    buf[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
    buf[4] = CRSF_ADDRESS_CRSF_TRANSMITTER;
    // Payload
    memcpy(&buf[5], data, len);
    // CRC - Starts at type, ends before CRC
    buf[5+len] = crsf_crc.calc(&buf[2], len + 3);

#ifdef PLATFORM_ESP32
    portENTER_CRITICAL(&FIFOmux);
#endif
    SerialOutFIFO.push(sizeof(buf));
    SerialOutFIFO.pushBytes(buf, sizeof(buf));
#ifdef PLATFORM_ESP32
    portEXIT_CRITICAL(&FIFOmux);
#endif
}

uint8_t CRSF::setLuaHiddenFlag(uint8_t id, bool value){
  luaHiddenFlags ^= (-value ^ luaHiddenFlags) & (1 << ((id-1)));
  return value;
}

void CRSF::getLuaTextSelectionStructToArray(const void * luaStruct, uint8_t *outarray){
    struct tagLuaItem_textSelection *p1 = (struct tagLuaItem_textSelection*)luaStruct;
    memcpy(outarray+4,p1->label1,strlen(p1->label1)+1);
    memcpy(outarray+4+(strlen(p1->label1)+1),p1->textOption,strlen(p1->textOption)+1);
    memcpy(outarray+4+(strlen(p1->label1)+1)+(strlen(p1->textOption)+1),&p1->luaProperties2,sizeof(p1->luaProperties2));
    memcpy(outarray+4+(strlen(p1->label1)+1)+(strlen(p1->textOption)+1)+sizeof(p1->luaProperties2)+1,p1->label2,strlen(p1->label2)+1);
    
    outarray[0] = p1->luaProperties1.id;
    outarray[1] = 0; //chunk
    outarray[2] = p1->luaProperties1.parent; //parent
    outarray[3] = p1->luaProperties1.type;
    outarray[3] += ((luaHiddenFlags >>((p1->luaProperties1.id)-1)) & 1)*128;
    //outarray[4+(strlen(p1->label1)+1)+(strlen(p1->textOption)+1)] = (uint8_t)luaValues[p1->luaProperties1.id];
}

void CRSF::getLuaCommandStructToArray(const void * luaStruct, uint8_t *outarray){
    struct tagLuaItem_command *p1 = (struct tagLuaItem_command*)luaStruct;
    memcpy(outarray+4,p1->label1,strlen(p1->label1)+1);
    memcpy(outarray+4+(strlen(p1->label1)+1),&p1->luaProperties2,sizeof(p1->luaProperties2));
    memcpy(outarray+4+(strlen(p1->label1)+1)+sizeof(p1->luaProperties2),p1->label2,strlen(p1->label2)+1);
    
    outarray[0] = p1->luaProperties1.id;
    outarray[1] = 0; //chunk
    outarray[2] = p1->luaProperties1.parent; //parent
    outarray[3] = p1->luaProperties1.type;
    outarray[3] += ((luaHiddenFlags >>((p1->luaProperties1.id)-1)) & 1)*128;
    //outarray[4+(strlen(p1->label1)+1)] = (uint8_t)luaValues[p1->luaProperties1.id];
}

void CRSF::getLuaUint8StructToArray(const void * luaStruct, uint8_t *outarray){
    struct tagLuaItem_uint8 *p1 = (struct tagLuaItem_uint8*)luaStruct;
    memcpy(outarray+4,p1->label1,strlen(p1->label1)+1);
    memcpy(outarray+4+(strlen(p1->label1)+1),&p1->luaProperties2,sizeof(p1->luaProperties2));
    memcpy(outarray+4+(strlen(p1->label1)+1)+sizeof(p1->luaProperties2)+1,p1->label2,strlen(p1->label2)+1);
    
    outarray[0] = p1->luaProperties1.id;
    outarray[1] = 0; //chunk
    outarray[2] = p1->luaProperties1.parent; //parent
    outarray[3] = p1->luaProperties1.type;
    outarray[3] += ((luaHiddenFlags >>((p1->luaProperties1.id)-1)) & 1)*128;
    //outarray[4+(strlen(p1->label1)+1)] = (uint8_t)luaValues[p1->luaProperties1.id];
}

void CRSF::getLuaUint16StructToArray(const void * luaStruct, uint8_t *outarray){
    struct tagLuaItem_uint16 *p1 = (struct tagLuaItem_uint16*)luaStruct;
    memcpy(outarray+4,p1->label1,strlen(p1->label1)+1);
    memcpy(outarray+4+(strlen(p1->label1)+1),&p1->luaProperties2,sizeof(p1->luaProperties2));
    memcpy(outarray+4+(strlen(p1->label1)+1)+sizeof(p1->luaProperties2)+2,p1->label2,strlen(p1->label2)+1);
    
    outarray[0] = p1->luaProperties1.id;
    outarray[1] = 0; //chunk
    outarray[2] = p1->luaProperties1.parent; //parent
    outarray[3] = p1->luaProperties1.type;
    outarray[3] += ((luaHiddenFlags >>((p1->luaProperties1.id)-1)) & 1)*128;
    //[4+(strlen(p1->label1)+1)] = (uint8_t)(luaValues[p1->luaProperties1.id] >> 8);
    //outarray[4+(strlen(p1->label1)+2)] = (uint8_t)luaValues[p1->luaProperties1.id];
}


void CRSF::getLuaStringStructToArray(const void * luaStruct, uint8_t *outarray){
    struct tagLuaItem_string *p1 = (struct tagLuaItem_string*)luaStruct;
    memcpy(outarray+4,p1->label1,strlen(p1->label1)+1);
    memcpy(outarray+4+(strlen(p1->label1)+1),p1->label2,strlen(p1->label2)+1);
    outarray[0] = p1->luaProperties1.id;
    outarray[1] = 0; //chunk
    outarray[2] = p1->luaProperties1.parent; //parent
    outarray[3] = p1->luaProperties1.type;
    outarray[3] += ((luaHiddenFlags >>((p1->luaProperties1.id)-1)) & 1)*128;
}
void CRSF::getLuaFolderStructToArray(const void * luaStruct, uint8_t *outarray){
    struct tagLuaItem_string *p1 = (struct tagLuaItem_string*)luaStruct;
    memcpy(outarray+4,p1->label1,strlen(p1->label1)+1);
    outarray[0] = p1->luaProperties1.id;
    outarray[1] = 0; //chunk
    outarray[2] = p1->luaProperties1.parent; //parent
    outarray[3] = p1->luaProperties1.type;
    outarray[3] += ((luaHiddenFlags >>((p1->luaProperties1.id)-1)) & 1)*128;
}

//sendCRSF param can take anytype of lua field settings
uint8_t CRSF::sendCRSFparam(crsf_frame_type_e frame,uint8_t fieldchunk, crsf_value_type_e dataType, const void * luaData, uint8_t wholePacketSize)
{
    if (!CRSF::CRSFstate)
    {
        return 0;
    }
    uint8_t LUArespLength;
    uint8_t chunks = 0;    
    uint8_t currentPacketSize;    
    
    /**
     *calculate how many chunks needed for this field 
     */
    chunks = ((wholePacketSize-2)/(CHUNK_MAX_NUMBER_OF_BYTES));
    if((wholePacketSize-2) % (CHUNK_MAX_NUMBER_OF_BYTES)){
        chunks = chunks + 1;
    }

    //calculate how much byte this packet contains
    if((chunks - (fieldchunk+1)) > 0){
        currentPacketSize = CHUNK_MAX_NUMBER_OF_BYTES;
    } else {
        if((wholePacketSize-2) % (CHUNK_MAX_NUMBER_OF_BYTES)){
            currentPacketSize = (wholePacketSize-2) % (CHUNK_MAX_NUMBER_OF_BYTES);
        } else {
            currentPacketSize = CHUNK_MAX_NUMBER_OF_BYTES;
        }
    }
    LUArespLength = 2+2+ currentPacketSize; //2 bytes of header, fieldsetup1(fieldid, fieldchunk),
                                        // chunk-ed packets below
                                        //fieldsetup1(fieldparent,fieldtype),field name, 
                                        //fieldsetup2(value,min,max,default),field unit
    //create outbuffer size
    uint8_t chunkBuffer[wholePacketSize] = {0};
    uint8_t outBuffer[currentPacketSize + 5 + 2 + 2] = {0}; 
        //it is byte op, we can use memcpy with index to
        // destination memory.
    switch(dataType){
        case CRSF_TEXT_SELECTION:
        {
            getLuaTextSelectionStructToArray(luaData, chunkBuffer);
            break;
        }
        case CRSF_COMMAND:
        {
            getLuaCommandStructToArray(luaData, chunkBuffer);
            break;
        }
        case CRSF_UINT8:
        {
            getLuaUint8StructToArray(luaData,chunkBuffer);
            break;
        }
        case CRSF_UINT16:
        {
            getLuaUint16StructToArray(luaData,chunkBuffer);
            break;
        }
    // we dont have to include this for now. since we dont need it yet?
        case CRSF_INT8:
        {
            //getLuaint8StructToArray(luaData,chunkBuffer);
            break;
        }
        case CRSF_INT16:
        {
            //getLuaint16StructToArray(luaData,chunkBuffer);
            break;
        }
        case CRSF_FLOAT:
        {
            //getLuaFloatStructToArray(luaData,chunkBuffer);
            break;
        }
    //
        case CRSF_STRING:
        case CRSF_INFO:
        {
            getLuaStringStructToArray(luaData,chunkBuffer);
            break;
        }
        case CRSF_FOLDER:
        {
            getLuaFolderStructToArray(luaData,chunkBuffer);
            break;
        }
        case CRSF_OUT_OF_RANGE:
        default:
        break;

    }
        memcpy(outBuffer+7,chunkBuffer+2+((fieldchunk*CHUNK_MAX_NUMBER_OF_BYTES)),currentPacketSize);
        outBuffer[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
        outBuffer[1] = LUArespLength + 2;   //received as #data in lua
        outBuffer[2] = frame; //received as command in lua
        // all below received as data in lua
        outBuffer[3] = CRSF_ADDRESS_RADIO_TRANSMITTER;
        outBuffer[4] = CRSF_ADDRESS_CRSF_TRANSMITTER;

        outBuffer[5] = chunkBuffer[0];
        outBuffer[6] = ((chunks - (fieldchunk+1))); //remaining chunk to send;

        uint8_t crc = crsf_crc.calc(&outBuffer[2], LUArespLength + 1);
        outBuffer[LUArespLength + 3] = crc;
    
#ifdef PLATFORM_ESP32
    portENTER_CRITICAL(&FIFOmux);
#endif
    SerialOutFIFO.push(LUArespLength + 4);
    SerialOutFIFO.pushBytes(outBuffer, LUArespLength + 4);
#ifdef PLATFORM_ESP32
    portEXIT_CRITICAL(&FIFOmux);
#endif
    return ((chunks - (fieldchunk+1)));
}

void ICACHE_RAM_ATTR CRSF::sendTelemetryToTX(uint8_t *data)
{
    if (data[CRSF_TELEMETRY_LENGTH_INDEX] > CRSF_PAYLOAD_SIZE_MAX)
    {
        ERRLN("too large");
        return;
    }

    if (CRSF::CRSFstate)
    {
        data[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
#ifdef PLATFORM_ESP32
        portENTER_CRITICAL(&FIFOmux);
#endif
        SerialOutFIFO.push(CRSF_FRAME_SIZE(data[CRSF_TELEMETRY_LENGTH_INDEX])); // length
        SerialOutFIFO.pushBytes(data, CRSF_FRAME_SIZE(data[CRSF_TELEMETRY_LENGTH_INDEX]));
#ifdef PLATFORM_ESP32
        portEXIT_CRITICAL(&FIFOmux);
#endif
    }
}

void ICACHE_RAM_ATTR CRSF::setSyncParams(uint32_t PacketInterval)
{
    CRSF::RequestedRCpacketInterval = PacketInterval;
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
    CRSF::SyncWaitPeriodCounter = millis();
    CRSF::OpenTXsyncOffsetSafeMargin = 1000;
    LPF_OPENTX_SYNC_OFFSET.init(0);
    LPF_OPENTX_SYNC_MARGIN.init(0);
#endif
}

uint32_t ICACHE_RAM_ATTR CRSF::GetRCdataLastRecv()
{
    return CRSF::RCdataLastRecv;
}

void ICACHE_RAM_ATTR CRSF::JustSentRFpacket()
{
    CRSF::OpenTXsyncOffset = micros() - CRSF::RCdataLastRecv;

    if (CRSF::OpenTXsyncOffset > (int32_t)CRSF::RequestedRCpacketInterval) // detect overrun case when the packet arrives too late and caculate negative offsets.
    {
        CRSF::OpenTXsyncOffset = -(CRSF::OpenTXsyncOffset % CRSF::RequestedRCpacketInterval);
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
        // wait until we stablize after changing pkt rate
        if (millis() > (CRSF::SyncWaitPeriodCounter + AutoSyncWaitPeriod))
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
    //DBGLN("%d, %d", CRSF::OpenTXsyncOffset, CRSF::OpenTXsyncOffsetSafeMargin / 10);
}

void CRSF::disableOpentxSync()
{
    OpentxSyncActive = false;
}

void CRSF::enableOpentxSync()
{
    OpentxSyncActive = true;
}

void ICACHE_RAM_ATTR CRSF::sendSyncPacketToTX() // in values in us.
{
    uint32_t now = millis();
    if (CRSF::CRSFstate && now >= (OpenTXsyncLastSent + OpenTXsyncPacketInterval))
    {
        uint32_t packetRate;
        if (CRSF::UARTcurrentBaud == 115200 && CRSF::RequestedRCpacketInterval == 2000)
        {
            packetRate = 40000; //constrain to 250hz max
        }
        else
        {
            packetRate = CRSF::RequestedRCpacketInterval * 10; //convert from us to right format
        }

        int32_t offset = CRSF::OpenTXsyncOffset * 10 - CRSF::OpenTXsyncOffsetSafeMargin; // + 400us offset that that opentx always has some headroom

        struct otxSyncData {
            uint8_t extendedType; // CRSF_FRAMETYPE_OPENTX_SYNC
            uint32_t rate; // Big-Endian
            uint32_t offset; // Big-Endian
        } PACKED;
        
        uint8_t buffer[sizeof(otxSyncData)];
        struct otxSyncData * const sync = (struct otxSyncData * const)buffer;

        sync->extendedType = CRSF_FRAMETYPE_OPENTX_SYNC;
        sync->rate = htobe32(packetRate);
        sync->offset = htobe32(offset);

        packetQueueExtended(CRSF_FRAMETYPE_RADIO_ID, buffer, sizeof(buffer));

        OpenTXsyncLastSent = now;
    }
}

bool ICACHE_RAM_ATTR CRSF::ProcessPacket()
{
    if (CRSFstate == false)
    {
        CRSFstate = true;
        DBGLN("CRSF UART Connected");

#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
        SyncWaitPeriodCounter = millis(); // set to begin wait for auto sync offset calculation
        LPF_OPENTX_SYNC_MARGIN.init(0);
        LPF_OPENTX_SYNC_OFFSET.init(0);
#endif // FEATURE_OPENTX_SYNC_AUTOTUNE
        hasEverConnected = true;
        connected();
    }
    
    const uint8_t packetType = CRSF::inBuffer.asRCPacket_t.header.type;

    if (packetType == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
        CRSF::RCdataLastRecv = micros();
        GetChannelDataIn();
        return true;
    }
    else if (packetType == CRSF_FRAMETYPE_MSP_REQ || packetType == CRSF_FRAMETYPE_MSP_WRITE)
    {
        volatile uint8_t *SerialInBuffer = CRSF::inBuffer.asUint8_t;
        const uint8_t length = CRSF::inBuffer.asRCPacket_t.header.frame_size + 2;
        AddMspMessage(length, SerialInBuffer);
        return true;
    } else {
        const volatile uint8_t *SerialInBuffer = CRSF::inBuffer.asUint8_t;
        if ((SerialInBuffer[3] == CRSF_ADDRESS_CRSF_TRANSMITTER || SerialInBuffer[3] == CRSF_ADDRESS_BROADCAST) &&
            (SerialInBuffer[4] == CRSF_ADDRESS_RADIO_TRANSMITTER || SerialInBuffer[4] == CRSF_ADDRESS_ELRS_LUA))
        {
            if(SerialInBuffer[4] == CRSF_ADDRESS_ELRS_LUA){
                elrsLUAmode = true;
            } else {
                elrsLUAmode = false;
            }
            if (packetType == CRSF_FRAMETYPE_COMMAND && 
                SerialInBuffer[5] == SUBCOMMAND_CRSF &&
                SerialInBuffer[6] == COMMAND_MODEL_SELECT_ID) {
                    modelId = SerialInBuffer[7];
                    RecvModelUpdate();
                    return true;
            }
            ParameterUpdateData[0] = packetType;
            ParameterUpdateData[1] = SerialInBuffer[5];
            ParameterUpdateData[2] = SerialInBuffer[6];
            RecvParameterUpdate();
            return true;
        }
        DBGLN("Got Other Packet");        
        //GoodPktsCount++;        
    }
    return false;
}

uint8_t* ICACHE_RAM_ATTR CRSF::GetMspMessage()
{
    if (MspDataLength > 0)
    {
        return MspData;
    }
    return NULL;
}

void ICACHE_RAM_ATTR CRSF::ResetMspQueue()
{
    MspWriteFIFO.flush();
    MspDataLength = 0;
    memset(MspData, 0, ELRS_MSP_BUFFER);
}

void ICACHE_RAM_ATTR CRSF::UnlockMspMessage()
{
    // current msp message is sent so restore next buffered write
    if (MspWriteFIFO.peek() > 0)
    {
        uint8_t length = MspWriteFIFO.pop();
        MspDataLength = length;
        MspWriteFIFO.popBytes(MspData, length);
    }
    else
    {
        // no msp message is ready to send currently
        MspDataLength = 0;
        memset(MspData, 0, ELRS_MSP_BUFFER);
    }
}

void ICACHE_RAM_ATTR CRSF::AddMspMessage(mspPacket_t* packet)
{
    if (packet->payloadSize > ENCAPSULATED_MSP_PAYLOAD_SIZE)
    {
        return;
    }

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
    AddMspMessage(totalBufferLen, outBuffer);
}

void ICACHE_RAM_ATTR CRSF::AddMspMessage(const uint8_t length, volatile uint8_t* data)
{
    if (length > ELRS_MSP_BUFFER)
    {
        return;
    }

    // store next msp message
    if (MspDataLength == 0)
    {
        for (uint8_t i = 0; i < length; i++)
        {
            MspData[i] = data[i];
        }
        MspDataLength = length;
    }
    // store all write requests since an update does send multiple writes
    else
    {
        MspWriteFIFO.push(length);
        for (uint8_t i = 0; i < length; i++)
        {
            MspWriteFIFO.push(data[i]);
        }
    }
}

void ICACHE_RAM_ATTR CRSF::handleUARTin()
{
    volatile uint8_t *SerialInBuffer = CRSF::inBuffer.asUint8_t;

    if (UARTwdt())
    {
        return;
    }

    while (CRSF::Port.available())
    {
        unsigned char const inChar = CRSF::Port.read();

        if (CRSFframeActive == false)
        {
            // stage 1 wait for sync byte //
            if (inChar == CRSF_ADDRESS_CRSF_TRANSMITTER ||
                inChar == CRSF_SYNC_BYTE)
            {
                // we got sync, reset write pointer
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
            if (SerialInPacketPtr > CRSF_MAX_PACKET_LEN - 1)
            {
                // we reached the maximum allowable packet length, so start again because shit fucked up hey.
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
                    GoodPktsCount++;
                    if (ProcessPacket())
                    {
                        //delayMicroseconds(50);
                        handleUARTout();
                        RCdataCallback();
                    }
                }
                else
                {
                    DBGLN("UART CRC failure");
                    // cleanup input buffer
                    flush_port_input();
                    BadPktsCount++;
                }
                CRSFframeActive = false;
                SerialInPacketPtr = 0;
                SerialInPacketLen = 0;
            }
        }
    }
}

void ICACHE_RAM_ATTR CRSF::handleUARTout()
{
    // both static to split up larger packages
    static uint8_t packageLength = 0;
    static uint8_t sendingOffset = 0;
    uint8_t writeLength = 0;

    if (OpentxSyncActive)
    {
        sendSyncPacketToTX(); // calculate mixer sync packet if needed
    }

    // check if we have data in the output FIFO that needs to be written or a large package was split up and we need to send the second part
    if (sendingOffset > 0 || SerialOutFIFO.peek() > 0) {
        duplex_set_TX();

#ifdef PLATFORM_ESP32
        portENTER_CRITICAL(&FIFOmux); // stops other tasks from writing to the FIFO when we want to read it
#endif
        // no package is in transit so get new data from the fifo
        if (sendingOffset == 0) {
            packageLength = SerialOutFIFO.pop();
            SerialOutFIFO.popBytes(CRSFoutBuffer, packageLength);
        }

        // if the package is long we need to split it up so it fits in the sending interval
        if (packageLength > MAX_BYTES_SENT_IN_UART_OUT) {
            writeLength = MAX_BYTES_SENT_IN_UART_OUT;
        } else {
            writeLength = packageLength;
        }


#ifdef PLATFORM_ESP32
        portEXIT_CRITICAL(&FIFOmux); // stops other tasks from writing to the FIFO when we want to read it
#endif

        // write the packet out, if it's a large package the offset holds the starting position
        CRSF::Port.write(CRSFoutBuffer + sendingOffset, writeLength);
        CRSF::Port.flush();

        sendingOffset += writeLength;
        packageLength -= writeLength;

        // after everything was writen reset the offset so a new package can be fetched from the fifo
        if (packageLength == 0) {
            sendingOffset = 0;
        }
        duplex_set_RX();

        // make sure there is no garbage on the UART left over
        flush_port_input();
    }
}

void ICACHE_RAM_ATTR CRSF::duplex_set_RX()
{
#ifdef PLATFORM_ESP32
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
#elif defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    digitalWrite(GPIO_PIN_BUFFER_OE, LOW ^ GPIO_PIN_BUFFER_OE_INVERTED);
#elif (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    CRSF::Port.enableHalfDuplexRx();
#endif
}

void ICACHE_RAM_ATTR CRSF::duplex_set_TX()
{
#ifdef PLATFORM_ESP32
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
#elif defined(GPIO_PIN_BUFFER_OE) && (GPIO_PIN_BUFFER_OE != UNDEF_PIN)
    digitalWrite(GPIO_PIN_BUFFER_OE, HIGH ^ GPIO_PIN_BUFFER_OE_INVERTED);
#elif (GPIO_PIN_RCSIGNAL_TX == GPIO_PIN_RCSIGNAL_RX)
    // writing to the port switches the mode
#endif
}

bool CRSF::UARTwdt()
{
    uint32_t now = millis();
    bool retval = false;
    if (now >= (UARTwdtLastChecked + UARTwdtInterval))
    {
        if (BadPktsCount >= GoodPktsCount)
        {
            DBGLN("Too many bad UART RX packets!");

            if (CRSFstate == true)
            {
                DBGLN("CRSF UART Disconnected");
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
                SyncWaitPeriodCounter = now; // set to begin wait for auto sync offset calculation
                CRSF::OpenTXsyncOffsetSafeMargin = 1000;
                CRSF::OpenTXsyncOffset = 0;
                CRSF::OpenTXsyncLastSent = 0;
#endif
                disconnected();
                CRSFstate = false;
            }

            uint32_t UARTrequestedBaud = (UARTcurrentBaud == CRSF_OPENTX_FAST_BAUDRATE) ?
                CRSF_OPENTX_SLOW_BAUDRATE : CRSF_OPENTX_FAST_BAUDRATE;

            DBGLN("UART WDT: Switch to: %d baud", UARTrequestedBaud);

            SerialOutFIFO.flush();
#ifdef PLATFORM_ESP32
            CRSF::Port.flush();
            CRSF::Port.updateBaudRate(UARTrequestedBaud);
#else
            CRSF::Port.begin(UARTrequestedBaud);
            #if defined(TARGET_TX_GHOST)
            USART1->CR1 &= ~USART_CR1_UE;
            USART1->CR3 |= USART_CR3_HDSEL;
            USART1->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV | USART_CR2_SWAP; //inverted/swapped
            USART1->CR1 |= USART_CR1_UE;
            #endif
            #if defined(TARGET_TX_FM30_MINI)
            LL_GPIO_SetPinPull(GPIOA, GPIO_PIN_2, LL_GPIO_PULL_DOWN); // default is PULLUP
            USART2->CR1 &= ~USART_CR1_UE;
            USART2->CR2 |= USART_CR2_RXINV | USART_CR2_TXINV; //inverted
            USART2->CR1 |= USART_CR1_UE;
            #endif
#endif
            UARTcurrentBaud = UARTrequestedBaud;
            duplex_set_RX();
            // cleanup input buffer
            flush_port_input();

            retval = true;
        }
        DBGLN("UART STATS Bad:Good = %u:%u", BadPktsCount, GoodPktsCount);

        UARTwdtLastChecked = now;
        GoodPktsCountResult = GoodPktsCount;
        BadPktsCountResult = BadPktsCount;
        BadPktsCount = 0;
        GoodPktsCount = 0;
    }
    return retval;
}

#ifdef PLATFORM_ESP32
//RTOS task to read and write CRSF packets to the serial port
void ICACHE_RAM_ATTR CRSF::ESP32uartTask(void *pvParameters)
{
    DBGLN("ESP32 CRSF UART LISTEN TASK STARTED");
    CRSF::Port.begin(CRSF_OPENTX_FAST_BAUDRATE, SERIAL_8N1,
                     GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX,
                     false, 500);
    CRSF::duplex_set_RX();
    vTaskDelay(500);
    flush_port_input();
    (void)pvParameters;
    for (;;)
    {
        handleUARTin();
    }
}
#endif // PLATFORM_ESP32

#elif CRSF_RX_MODULE // !CRSF_TX_MODULE
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

void ICACHE_RAM_ATTR CRSF::sendMSPFrameToFC(uint8_t* data)
{
    const uint8_t totalBufferLen = 14;

    // SerialOutFIFO.push(totalBufferLen);
    // SerialOutFIFO.pushBytes(outBuffer, totalBufferLen);
    this->_dev->write(data, totalBufferLen);
}
#endif // CRSF_TX_MODULE


void ICACHE_RAM_ATTR CRSF::GetChannelDataIn() // data is packed as 11 bits per channel
{
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
}

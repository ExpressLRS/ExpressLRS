#include "CRSF.h"
#include "../../lib/FIFO/FIFO.h"
#include "telemetry_protocol.h"
#include "channels.h"

//#define DEBUG_CRSF_NO_OUTPUT // debug, don't send RC msgs over UART

#ifdef PLATFORM_ESP32
void ESP32uartTask(void *pvParameters);
#endif

GENERIC_CRC8 crsf_crc(CRSF_CRC_POLY);

volatile crsf_channels_s CRSFBase::PackedRCdataOut;
volatile crsfPayloadLinkstatistics_s CRSFBase::LinkStatistics;

#if CRSF_TX_MODULE

CRSF_TXModule crsfTx;

static void nullCallback(void) {}

void (*CRSF_TXModule::disconnected)() = nullCallback; // called when CRSF stream is lost
void (*CRSF_TXModule::connected)() = nullCallback;    // called when CRSF stream is regained
void (*CRSF_TXModule::RecvParameterUpdate)() = nullCallback; // called when recv parameter update req, ie from LUA

/// UART Handling ///
uint32_t CRSF_TXModule::GoodPktsCountResult = 0;
uint32_t CRSF_TXModule::BadPktsCountResult = 0;
uint32_t CRSF_TXModule::GoodPktsCount = 0;
uint32_t CRSF_TXModule::BadPktsCount = 0;

volatile bool CRSF_TXModule::CRSFframeActive = false; //since we get a copy of the serial data use this flag to know when to ignore it

volatile uint8_t CRSF_TXModule::SerialInPacketLen = 0; // length of the CRSF packet as measured
volatile uint8_t CRSF_TXModule::SerialInPacketPtr = 0; // index where we are reading/writing
volatile uint8_t CRSF_TXModule::ParameterUpdateData[2] = {0};

volatile inBuffer_U CRSF_TXModule::inBuffer;

bool CRSF_TXModule::CRSFstate = false;

///Out FIFO to buffer messages///
FIFO MspWriteFIFO;

uint8_t CRSF_TXModule::MspData[ELRS_MSP_BUFFER] = {0};
uint8_t CRSF_TXModule::MspDataLength = 0;
volatile uint8_t CRSF_TXModule::MspRequestsInTransit = 0;
uint32_t CRSF_TXModule::LastMspRequestSent = 0;

//TODO: needs to be somewhere else
uint32_t UARTwdtLastChecked;
uint32_t UARTcurrentBaud;

void CRSF_TXModule::begin(TransportLayer* dev)
{
  TXModule::begin(dev);
}

void ICACHE_RAM_ATTR CRSF_TXModule::sendLinkStatisticsToTX()
{
    if (!CRSF_TXModule::CRSFstate)
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

    if (_dev) _dev->sendAsync(outBuffer, LinkStatisticsFrameLength + 4);
}

void CRSF_TXModule::sendLUAresponse(uint8_t val[], uint8_t len)
{
    if (!CRSF_TXModule::CRSFstate)
    {
        return;
    }

    //
    // TODO: this code is asking for troubles!!!
    //
    uint8_t LUArespLength = len + 2;
    uint8_t outBuffer[LUArespLength + 5];// = {0};

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

    if (_dev) _dev->sendAsync(outBuffer, LUArespLength + 4);
}

void ICACHE_RAM_ATTR CRSF_TXModule::sendTelemetryToTX(uint8_t *data)
{
    if (data[2] == CRSF_FRAMETYPE_MSP_RESP)
    {
        MspRequestsInTransit--;
    }

    if (data[CRSF_TELEMETRY_LENGTH_INDEX] > CRSF_PAYLOAD_SIZE_MAX)
    {
        Serial.print("too large");
        return;
    }

    if (CRSF_TXModule::CRSFstate)
    {
        data[0] = CRSF_ADDRESS_RADIO_TRANSMITTER;
        if (_dev) _dev->sendAsync(data, CRSF_FRAME_SIZE(data[CRSF_TELEMETRY_LENGTH_INDEX]));
    }
}


void ICACHE_RAM_ATTR CRSF_TXModule::sendSyncPacketToTX() // in values in us.
{
    uint32_t now = millis();
    if (CRSF_TXModule::CRSFstate && now >= (syncLastSent + OpenTXsyncPacketInterval))
    {
        uint32_t packetRate = packetInterval * 10; //convert from us to right format
        int32_t offset = syncOffset * 10 - syncOffsetSafeMargin; // + 400us offset that that opentx always has some headroom

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

        if (_dev) _dev->sendAsync(outBuffer, OpenTXsyncFrameLength + 4);
        syncLastSent = now;
    }
}



bool ICACHE_RAM_ATTR CRSF_TXModule::ProcessPacket(volatile uint16_t* channels)
{
    if (CRSFstate == false)
    {
        CRSFstate = true;
        Serial.println("CRSF UART Connected");

#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
        syncWaitPeriodCounter = millis(); // set to begin wait for auto sync offset calculation
        LPF_OPENTX_SYNC_MARGIN.init(0);
        LPF_OPENTX_SYNC_OFFSET.init(0);
#endif // FEATURE_OPENTX_SYNC_AUTOTUNE
        connected();
    }

    const uint8_t packetType = CRSF_TXModule::inBuffer.asRCPacket_t.header.type;

    if (packetType == CRSF_FRAMETYPE_PARAMETER_WRITE)
    {
        const volatile uint8_t *SerialInBuffer = CRSF_TXModule::inBuffer.asUint8_t;
        if (SerialInBuffer[3] == CRSF_ADDRESS_CRSF_TRANSMITTER &&
            SerialInBuffer[4] == CRSF_ADDRESS_RADIO_TRANSMITTER)
        {
            ParameterUpdateData[0] = SerialInBuffer[5];
            ParameterUpdateData[1] = SerialInBuffer[6];
            RecvParameterUpdate();
            return true;
        }
        Serial.println("Got Other Packet");
    }
    else if (packetType == CRSF_FRAMETYPE_RC_CHANNELS_PACKED)
    {
        onChannelDataIn();
        GoodPktsCount++;
        GetChannelDataIn(channels);
        return true;
    }
    else if (packetType == CRSF_FRAMETYPE_MSP_REQ || packetType == CRSF_FRAMETYPE_MSP_WRITE)
    {
        volatile uint8_t *SerialInBuffer = CRSF_TXModule::inBuffer.asUint8_t;
        const uint8_t length = CRSF_TXModule::inBuffer.asRCPacket_t.header.frame_size + 2;
        AddMspMessage(length, SerialInBuffer);
        return true;
    }
    return false;
}

uint8_t* ICACHE_RAM_ATTR CRSF_TXModule::GetMspMessage()
{
    if (MspDataLength > 0)
    {
        return MspData;
    }
    return NULL;
}

void ICACHE_RAM_ATTR CRSF_TXModule::ResetMspQueue()
{
    MspWriteFIFO.flush();
    MspDataLength = 0;
    MspRequestsInTransit = 0;
    memset(MspData, 0, ELRS_MSP_BUFFER);
}

void ICACHE_RAM_ATTR CRSF_TXModule::UnlockMspMessage()
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

void ICACHE_RAM_ATTR CRSF_TXModule::AddMspMessage(mspPacket_t* packet)
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

void ICACHE_RAM_ATTR CRSF_TXModule::AddMspMessage(const uint8_t length, volatile uint8_t* data)
{
    uint32_t now = millis();
    if (length > ELRS_MSP_BUFFER)
    {
        return;
    }

    // only store one CRSF_FRAMETYPE_MSP_REQ
    if ((MspRequestsInTransit > 0 && data[2] == CRSF_FRAMETYPE_MSP_REQ))
    {
        if (LastMspRequestSent + ELRS_MSP_REQ_TIMEOUT_MS < now)
        {
            MspRequestsInTransit = 0;
        }
        else
        {
            return;
        }
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

    if (data[2] == CRSF_FRAMETYPE_MSP_REQ)
    {
        MspRequestsInTransit++;
        LastMspRequestSent = now;
    }
}

void ICACHE_RAM_ATTR CRSF_TXModule::consumeInputByte(uint8_t in, volatile uint16_t* channels)
{
  volatile uint8_t *SerialInBuffer = CRSF_TXModule::inBuffer.asUint8_t;

  if (CRSFframeActive == false) {
    // stage 1 wait for sync byte //
    if (in == CRSF_ADDRESS_CRSF_TRANSMITTER || in == CRSF_SYNC_BYTE) {
      // we got sync, reset write pointer
      SerialInPacketPtr = 0;
      SerialInPacketLen = 0;
      CRSFframeActive = true;
      SerialInBuffer[SerialInPacketPtr] = in;
      SerialInPacketPtr++;
    }
  } else {  // frame is active so we do the processing

    // first if things have gone wrong //
    if (SerialInPacketPtr > CRSF_MAX_PACKET_LEN - 1) {
      // we reached the maximum allowable packet length, so start again
      // because shit fucked up hey.
      SerialInPacketPtr = 0;
      SerialInPacketLen = 0;
      CRSFframeActive = false;
      return;
    }

    // special case where we save the expected pkt len to buffer //
    if (SerialInPacketPtr == 1) {
      if (in <= CRSF_MAX_PACKET_LEN) {
        SerialInPacketLen = in;
      } else {
        SerialInPacketPtr = 0;
        SerialInPacketLen = 0;
        CRSFframeActive = false;
        return;
      }
    }

    SerialInBuffer[SerialInPacketPtr] = in;
    SerialInPacketPtr++;

    if (SerialInPacketPtr >=
        (SerialInPacketLen +
         2))  // plus 2 because the packlen is referenced from the start of
              // the 'type' flag, IE there are an extra 2 bytes.
    {
      char CalculatedCRC =
          crsf_crc.calc((uint8_t *)SerialInBuffer + 2, SerialInPacketPtr - 3);

      if (CalculatedCRC == in) {
        if (ProcessPacket(channels)) {
          // delayMicroseconds(50);
          send();
        }
      } else {
        Serial.println("UART CRC failure");
        // cleanup input buffer
        if (_dev) _dev->flushInput();
        BadPktsCount++;
      }
      CRSFframeActive = false;
      SerialInPacketPtr = 0;
      SerialInPacketLen = 0;
    }
  }
}

void ICACHE_RAM_ATTR CRSF_TXModule::flushTxBuffers()
{
  if (_dev) _dev->flushOutput();
}


bool CRSF_TXModule::UARTwdt()
{
    uint32_t now = millis();
    bool retval = false;
    if (now >= (UARTwdtLastChecked + UARTwdtInterval))
    {
        if (BadPktsCount >= GoodPktsCount)
        {
            Serial.print("Too many bad UART RX packets! ");

            if (CRSFstate == true)
            {
                Serial.println("CRSF UART Disconnected");
#ifdef FEATURE_OPENTX_SYNC_AUTOTUNE
                SyncWaitPeriodCounter = now; // set to begin wait for auto sync offset calculation
                syncOffsetSafeMargin = 1000;
                syncOffset = 0;
                syncLastSent = 0;
#endif
                disconnected();
                CRSFstate = false;
            }

            uint32_t UARTrequestedBaud = (UARTcurrentBaud == CRSF_OPENTX_FAST_BAUDRATE) ?
                CRSF_OPENTX_SLOW_BAUDRATE : CRSF_OPENTX_FAST_BAUDRATE;

            Serial.print("UART WDT: Switch to: ");
            Serial.print(UARTrequestedBaud);
            Serial.println(" baud");

            // re-init with a different baud rate
            if (_dev) _dev->updateBaudRate(UARTrequestedBaud);
            UARTcurrentBaud = UARTrequestedBaud;

            //duplex_set_RX();
            
            // cleanup input buffer
            if (_dev) _dev->flushInput();

            retval = true;
        }
        Serial.print("UART STATS Bad:Good = ");
        Serial.print(BadPktsCount);
        Serial.print(":");
        Serial.println(GoodPktsCount);

        UARTwdtLastChecked = now;
        GoodPktsCountResult = GoodPktsCount;
        BadPktsCountResult = BadPktsCount;
        BadPktsCount = 0;
        GoodPktsCount = 0;
    }
    return retval;
}

void ICACHE_RAM_ATTR CRSF_TXModule::GetChannelDataIn(
    volatile uint16_t *channels)  // data is packed as 11 bits per channel
{
  if (!channels) return;

  const volatile crsf_channels_t *rcChannels =
      &CRSF_TXModule::inBuffer.asRCPacket_t.channels;

  channels[0] = (rcChannels->ch0);
  channels[1] = (rcChannels->ch1);
  channels[2] = (rcChannels->ch2);
  channels[3] = (rcChannels->ch3);
  channels[4] = (rcChannels->ch4);
  channels[5] = (rcChannels->ch5);
  channels[6] = (rcChannels->ch6);
  channels[7] = (rcChannels->ch7);
  channels[8] = (rcChannels->ch8);
  channels[9] = (rcChannels->ch9);
  channels[10] = (rcChannels->ch10);
  channels[11] = (rcChannels->ch11);
  channels[12] = (rcChannels->ch12);
  channels[13] = (rcChannels->ch13);
  channels[14] = (rcChannels->ch14);
  channels[15] = (rcChannels->ch15);

  updateSwitchValues(channels);
}

#elif CRSF_RX_MODULE // !CRSF_TX_MODULE

CRSF_RXModule crsfRx;

void CRSF_RXModule::begin(TransportLayer* dev)
{
  _dev = dev;
}

// Sent ASYNC
void ICACHE_RAM_ATTR CRSF_RXModule::sendLinkStatisticsToFC()
{
    uint8_t outBuffer[LinkStatisticsFrameLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = LinkStatisticsFrameLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;

    memcpy(outBuffer + 3, (byte *)&LinkStatistics, LinkStatisticsFrameLength);

    uint8_t crc = crsf_crc.calc(&outBuffer[2], LinkStatisticsFrameLength + 1);

    outBuffer[LinkStatisticsFrameLength + 3] = crc;

#ifndef DEBUG_CRSF_NO_OUTPUT
    if (_dev) _dev->sendAsync(outBuffer, LinkStatisticsFrameLength + 4);
#endif
}

// Sent SYNC
void ICACHE_RAM_ATTR CRSF_RXModule::sendRCFrameToFC()
{
    uint8_t outBuffer[RCframeLength + 4] = {0};

    outBuffer[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    outBuffer[1] = RCframeLength + 2;
    outBuffer[2] = CRSF_FRAMETYPE_RC_CHANNELS_PACKED;

    memcpy(outBuffer + 3, (byte *)&PackedRCdataOut, RCframeLength);

    uint8_t crc = crsf_crc.calc(&outBuffer[2], RCframeLength + 1);

    outBuffer[RCframeLength + 3] = crc;
#ifndef DEBUG_CRSF_NO_OUTPUT
    if (_dev) _dev->write(outBuffer, RCframeLength + 4);
#endif
}

// Sent SYNC
void ICACHE_RAM_ATTR CRSF_RXModule::sendMSPFrameToFC(uint8_t* data)
{
    const uint8_t totalBufferLen = 14;
    if (_dev) _dev->write(data, totalBufferLen);
}
#endif // !CRSF_TX_MODULE && !CRSF_RX_MODULE

#include "SerialSmartAudio.h"

#include "freqTable.h"
#include "msptypes.h"
#if defined(PLATFORM_ESP32)
#include <hal/uart_ll.h>
#endif

#define SMARTAUDIO_MAX_FRAME_SIZE 32
#define SMARTAUDIO_HEADER_DUMMY 0x00 // Page 2: "The SmartAudio line need to be low before a frame is sent. If the host MCU can’t handle this it can be done by sending a 0x00 dummy byte in front of the actual frame."
#define SMARTAUDIO_HEADER_1 0xAA
#define SMARTAUDIO_HEADER_2 0x55
#define SMARTAUDIO_CRC_POLY 0xD5
#define SMARTAUDIO_RESPONSE_DELAY_MS 200

// check value for MSP_SET_VTX_CONFIG to determine if value is encoded
// band/channel or frequency in MHz (3 bits for band and 3 bits for channel)
#define VTXCOMMON_MSP_BANDCHAN_CHKVAL ((uint16_t)((7 << 3) + 7))

GENERIC_CRC8 crc(SMARTAUDIO_CRC_POLY);

SerialSmartAudio::SerialSmartAudio(Stream &out, Stream &in, int8_t serial1TXpin) : SerialIO(&out, &in)
{
#if defined(PLATFORM_ESP32)
    // we are on UART1, use Serial1 TX assigned pin for half duplex
    UTXDoutIdx = U1TXD_OUT_IDX;
    URXDinIdx = U1RXD_IN_IDX;
    halfDuplexPin = serial1TXpin;
#endif
    setRXMode();
    crsfRouter.addConnector(this);
}

void SerialSmartAudio::setTXMode() const
{
#if defined(PLATFORM_ESP32)
    pinMode(halfDuplexPin, OUTPUT);                                 // set half duplex GPIO to OUTPUT
    digitalWrite(halfDuplexPin, HIGH);                              // set half duplex GPIO to high level
    pinMatrixOutAttach(halfDuplexPin, UTXDoutIdx, false, false);    // attach GPIO as output of UART TX
#endif
}

void SerialSmartAudio::setRXMode() const
{
#if defined(PLATFORM_ESP32)
    pinMode(halfDuplexPin, INPUT_PULLUP);                           // set half duplex GPIO to INPUT
    pinMatrixInAttach(halfDuplexPin, URXDinIdx, false);             // attach half duplex GPIO as input to UART RX
#endif
}

void SerialSmartAudio::sendQueuedData(uint32_t maxBytesToSend)
{
#if defined(PLATFORM_ESP32)
    uint32_t bytesWritten = 0;
    static unsigned long lastSendTime = 0; // we need to delay between sending frames to allow for responses
    while (millis() - lastSendTime > SMARTAUDIO_RESPONSE_DELAY_MS && _fifo.size() > 0 && bytesWritten < maxBytesToSend) // OVTX only changes protocols on startup every 500ms; if we send our 3 packets in different 500ms windows, we have a better chance of success
    {
        _fifo.lock();
        uint8_t frameSize = _fifo.pop() - 1;
        uint8_t frame[frameSize];
        _fifo.popBytes(frame, frameSize);
        _fifo.unlock();
        setTXMode();
        _outputPort->write(frame, frameSize);
        bytesWritten += frameSize;
        lastSendTime = millis();
    }
    if (uart_ll_is_tx_idle(UART_LL_GET_HW(1)))
    {
        setRXMode();
    }
#endif
}

void SerialSmartAudio::forwardMessage(const crsf_header_t *message)
{
    auto data = (uint8_t *)message;
    // What we're handed here is MSP wrapped in CRSF, so our offsets are thrown off
    uint8_t innerLength = data[6];
    if (innerLength < 2)
    {
        return;
    }
    if (data[7] != MSP_SET_VTX_CONFIG)
    {
        return;
    }

    // We're gonna lie and queue the MSP frame by transforming it into a handful of SmartAudio frames
    uint16_t freq = 0;
    if (data[8] > VTXCOMMON_MSP_BANDCHAN_CHKVAL)
    {
        // This is a frequency in MHz
        freq = data[8] + (data[9] << 8);
    }
    else
    {
        freq = getFreqByIdx(data[8]);
    }
    
    if (freq == 0)
    {
        return;
    }

    /* SmartAudio Quirks
     * Page 2: "4800bps 1 Start bit and 2 Stop bit"
     * Page 2: "The SmartAudio line need to be low before a frame is sent. If the host MCU can’t handle this it can be done by sending a 0x00 dummy byte in front of the actual frame."
     * Page 4: "For all frames sent to the VTX the command byte needs to be shifted left by one bit and the LSB needs to be set"
     */

    // We need to construct SmartAudio packets from the MSP_SET_VTX_CONFIG packet
    uint8_t tempFrame[SMARTAUDIO_MAX_FRAME_SIZE];
    uint8_t frameIndex;
    uint8_t crcValue; // Needed to avoid sequence-point compiler warning by assigning / using frameIndex
    // Set frequency
    memset(tempFrame, 0, SMARTAUDIO_MAX_FRAME_SIZE);
    frameIndex = 0;
    tempFrame[frameIndex++] = SMARTAUDIO_HEADER_DUMMY;
    tempFrame[frameIndex++] = SMARTAUDIO_HEADER_1;
    tempFrame[frameIndex++] = SMARTAUDIO_HEADER_2;
    tempFrame[frameIndex++] = (0x04 << 1) | 0x01; // Command 0x04 is set frequency
    tempFrame[frameIndex++] = 0x02;               // Length of the payload
    tempFrame[frameIndex++] = (freq >> 8) & 0xFF;
    tempFrame[frameIndex++] = freq & 0xFF;
    crcValue = crc.calc(tempFrame, frameIndex);
    tempFrame[frameIndex++] = crcValue;
    _fifo.lock();
    _fifo.push(frameIndex + 1);
    _fifo.pushBytes(tempFrame, frameIndex);
    _fifo.unlock();

    // If packet has more than 4 bytes it also contains power idx and pitmode.
    bool havePowerAndPitmode = innerLength >= 4;
    if (havePowerAndPitmode)
    {
        uint8_t powerIndex = data[10];
        // Set power
        memset(tempFrame, 0, SMARTAUDIO_MAX_FRAME_SIZE);
        frameIndex = 0;
        tempFrame[frameIndex++] = SMARTAUDIO_HEADER_DUMMY;
        tempFrame[frameIndex++] = SMARTAUDIO_HEADER_1;
        tempFrame[frameIndex++] = SMARTAUDIO_HEADER_2;
        tempFrame[frameIndex++] = (0x02 << 1) | 0x01; // Command 0x02 is set power
        tempFrame[frameIndex++] = 0x01;               // Length of the payload
        tempFrame[frameIndex++] = powerIndex - 1;     // In SA2.1, we send a 0-n "power index"
        crcValue = crc.calc(tempFrame, frameIndex);
        tempFrame[frameIndex++] = crcValue;
        _fifo.lock();
        _fifo.push(frameIndex + 1);
        _fifo.pushBytes(tempFrame, frameIndex);
        _fifo.unlock();

        uint8_t pitmode = data[11];
        // Set pitmode
        memset(tempFrame, 0, SMARTAUDIO_MAX_FRAME_SIZE);
        frameIndex = 0;
        tempFrame[frameIndex++] = SMARTAUDIO_HEADER_DUMMY;
        tempFrame[frameIndex++] = SMARTAUDIO_HEADER_1;
        tempFrame[frameIndex++] = SMARTAUDIO_HEADER_2;
        tempFrame[frameIndex++] = (0x05 << 1) | 0x01;      // Command 0x05 is set power
        tempFrame[frameIndex++] = 0x01;                    // Length of the payload
        tempFrame[frameIndex++] = (pitmode ? 0x01 : 0x04); // bit 3 seems to be "clear pitmode" contrary to the docs; see BF, OpenVTX, etc.
        crcValue = crc.calc(tempFrame, frameIndex);
        tempFrame[frameIndex++] = crcValue;
        _fifo.lock();
        _fifo.push(frameIndex + 1);
        _fifo.pushBytes(tempFrame, frameIndex);
        _fifo.unlock();
    }
}
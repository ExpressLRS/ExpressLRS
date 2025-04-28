#pragma once

#include "targets.h"
#include "FIFO.h"

/**
 * @brief Abstract class that is to be extended by implementation classes for different serial protocols on the receiver side.
 *
 * At a minimum, a new protocol extension class should implement the following functions
 *
 * * sendRCFrame
 * * sendQueuedData
 * * processBytes
 */
class SerialIO {
public:

    SerialIO(Stream *output, Stream *input) : _outputPort(output), _inputPort(input) {}
    virtual ~SerialIO() = default;

    /**
     * @brief Set the Failsafe flag
     *
     * This allows the serial protocol implementation to react accordingly.
     * e.g. if it needs to set a flag in the serial messages, or stop sending over
     * serial port.
     *
     * @param failsafe true when the firmware has detected a failsafe condition.
     */
    void setFailsafe(bool failsafe);

    /**
     * @brief Signals the protocol to queue a link statistics packet
     *
     * The packet should be queued into the `_fifo` member variable as RC packets
     * are prioritised and ancillary data is sent after the RC data when the
     * `sendQueuedData` function is called.
     */
    virtual void queueLinkStatisticsPacket() = 0;

    /**
     * @brief send the RC channel data to the serial port stream `_outputPort` member
     * variable.
     *
     * If the function wishes to be called as fast as possible, then it should return
     * DURATION_IMMEDIATE, otherwise it should return the number of milliseconds delay
     * before this method is called again.
     *
     * @param frameAvailable indicates that a new OTA frame of data has been received
     * since the last call to this function
     * @param frameMissed indicates that a frame was not received in the OTA window
     * @param channelData pointer to the 16 channels of data
     * @return number of milliseconds to delay before this method is called again
     */
    virtual uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) = 0;

    /**
     * @brief send any previously queued data to the serial port stream `_outputPort`
     * member variable.
     *
     * This method is called each time around the main loop.
     */
    virtual void sendQueuedData(uint32_t maxBytesToSend);

    /**
     * @brief read bytes from the serial port and process them.
     *
     * The maximum number of bytes to read per call is obtained
     * from the `getMaxSerialReadSize` method call.
     *
     * This method *should* not be overridden by custom implementations, it is
     * only overridden by the `SerialNOOP` implementation.
     *
     * This method is called each time around the main loop.
     */
    virtual void processSerialInput();

    /**
     * @brief Get the maximum number of bytes to write to the serial port in each call.
     *
     * @return maximum number of bytes to write
     */
    virtual int getMaxSerialWriteSize() { return defaultMaxSerialWriteSize; }

    /**
     * @brief Returns true is the serial protocol driver wants to send RC packets immediately
     * in the "tock" timer callback rather than waiting for the serial timeout. For example,
     * CRSF protocol uses this to reduce jitter in teh RC commands being sent to the FC.
     *
     * @return true of the serial protocol driver wants to send RC packets in "tock"
     */
    virtual bool sendImmediateRC() { return false; }

protected:
    /// @brief the output stream for the serial port
    Stream *_outputPort;
    /// @brief flag that indicates the receiver is in the failsafe state
    bool failsafe = false;

    static const uint32_t SERIAL_OUTPUT_FIFO_SIZE = 256U;


    /**
     * @brief the FIFO that should be used to queue serial data to in the
     * `queueLinkStatisticsPacket` and `queueMSPFrameTransmission` method implementations.
     */
    FIFO<SERIAL_OUTPUT_FIFO_SIZE> _fifo;

    /**
     * @brief Get the maximum number of bytes to read from the serial port per call
     *
     * @return the maximum number of bytes to read
     */
    virtual int getMaxSerialReadSize() { return defaultMaxSerialReadSize; }

    /**
     * @brief Protocol specific method to process the bytes that have been read
     * from the serial port by the framework calling the `processSerialInput` method.
     *
     * @param bytes pointer to the byte array that contains the serial data
     * @param size number of bytes in the buffer
     */
    virtual void processBytes(uint8_t *bytes, uint16_t size) = 0;

private:
    const int defaultMaxSerialReadSize = 64;
    const int defaultMaxSerialWriteSize = 128;

    Stream *_inputPort;
};

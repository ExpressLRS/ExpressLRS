#pragma once

#include "targets.h"

/**
 * @brief Abstract class that is extended to provide an interface to a handset.
 *
 * There are three implementations of the Controller class
 *
 * - CRSFController - implements the CRSF protocol for communicating with the handset
 * - PPM Controller - PPM protocol, can be connected to the DSC/trainer port for simple non-CRSF handsets
 * - AutoDetect - this implementation uses an RMT channel to auto-detect a PPM or CRSF handset and swap the
 *   global controller variable to point to instance of the actual implementation. This allows a TX module
 *   to be moved between a CRSF capable handset and PPM only handset e.g. an EdgeTX radio and a surface radio.
 */
class Controller
{
public:
    /**
     * @brief Start the controller protocol
     */
    virtual void Begin() = 0;

    /**
     * @brief End the controller protocol
     */
    virtual void End() = 0;

    /**
     * @brief register a function to be called when the protocol has read an RC data packet from the handset
     * @param callback
     */
    void setRCDataCallback(void (*callback)()) { RCdataCallback = callback; }
    /**
     * @brief register a function to be called when a request to update a parameter is send from the handset
     * @param callback
     */
    void registerParameterUpdateCallback(void (*callback)(uint8_t type, uint8_t index, uint8_t arg)) { RecvParameterUpdate = callback; }
    /**
     * Register callback functions for state information about the connection or handset
     * @param connectedCallback called when the protocol detects a stable connection to the handset
     * @param disconnectedCallback called when the protocol loses its connection to the handset
     * @param RecvModelUpdateCallback called when the handset sends a message to set the current model number
     */
    void registerCallbacks(void (*connectedCallback)(), void (*disconnectedCallback)(), void (*RecvModelUpdateCallback)())
    {
        connected = connectedCallback;
        disconnected = disconnectedCallback;
        RecvModelUpdate = RecvModelUpdateCallback;
    }

    /**
     * @brief Process any pending input data from the handset
     */
    virtual void handleInput() = 0;

    /**
     * @return true if the protocol detects that the arming state is active
     */
    virtual bool IsArmed() = 0;

    /**
     * Called to set the expected packet interval from the handset.
     * This can be used to synchronise the packets from the handset.
     * @param PacketInterval in microseconds
     */
    virtual void setPacketInterval(int32_t PacketInterval) { RequestedRCpacketInterval = PacketInterval; }
    /**
     * @return the maximum number of bytes that the protocol can send to the handset in a single message
     */
    virtual uint8_t GetMaxPacketBytes() const { return 255; }

    /**
     * @brief Called to indicate to the protocol that a packet has just been sent over-the-air
     * This is used to synchronise the packets from the handset to the OTA protocol to minimise latency
     */
    virtual void JustSentRFpacket() {}
    /**
     * Send a telemetry packet back to the handset
     * @param data
     */
    virtual void sendTelemetryToTX(uint8_t *data) {}

    /**
     * @return the time in microseconds when the last RC packet was received from the handset
     */
    uint32_t GetRCdataLastRecv() const { return RCdataLastRecv; }

protected:
    virtual ~Controller() = default;

    bool controllerConnected = false;
    void (*RCdataCallback)() = nullptr;  // called when there is new RC data
    void (*disconnected)() = nullptr;    // called when RC packet stream is lost
    void (*connected)() = nullptr;       // called when RC packet stream is regained
    void (*RecvModelUpdate)() = nullptr; // called when model id changes, ie command from Radio
    void (*RecvParameterUpdate)(uint8_t type, uint8_t index, uint8_t arg) = nullptr; // called when recv parameter update req, ie from LUA

    volatile uint32_t RCdataLastRecv = 0;
    int32_t RequestedRCpacketInterval = 5000; // default to 200hz as per 'normal'
};

#ifdef TARGET_TX
extern Controller *controller;
#endif

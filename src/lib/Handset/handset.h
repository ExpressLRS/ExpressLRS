#pragma once

#include "common.h"

typedef void (*RcChannelsOverrideCallback_fn)(uint32_t channels[], size_t channelCnt);

/**
 * @brief Abstract class that is extended to provide an interface to a handset.
 *
 * There are three implementations of the Handset class
 *
 * - CRSFHandset - implements the CRSF protocol for communicating with the handset
 * - PPMHandset - PPM protocol, can be connected to the DSC/trainer port for simple non-CRSF handsets
 * - AutoDetect - this implementation uses an RMT channel to auto-detect a PPM or CRSF handset and swap the
 *   global `handset` variable to point to instance of the actual implementation. This allows a TX module
 *   to be moved between a CRSF capable handset and PPM only handset e.g. an EdgeTX radio and a surface radio.
 */
class Handset
{
public:
    /**
     * @brief Start the handset protocol
     */
    virtual void Begin() = 0;

    /**
     * @brief End the handset protocol
     */
    virtual void End() = 0;

    /**
     * @brief  Register callback used to allow overriding of channels when received before any actions are taken with them
     *         and before RCDataCallback
     */
    void setRcChannelsOverrideCallback(RcChannelsOverrideCallback_fn callback) { RcChannelsOverrideCallback = callback; }

    /**
     * @brief register a function to be called when the protocol has read an RC data packet from the handset
     * @param callback
     */
    void setRCDataCallback(void (*callback)()) { RCdataCallback = callback; }

    /**
     * Register callback functions for state information about the connection or handset
     * @param connectedCallback called when the protocol detects a stable connection to the handset
     * @param disconnectedCallback called when the protocol loses its connection to the handset
     */
    void registerCallbacks(void (*connectedCallback)(), void (*disconnectedCallback)())
    {
        connected = connectedCallback;
        disconnected = disconnectedCallback;
    }

    /**
     * @brief Process any pending input data from the handset
     */
    virtual void handleInput() = 0;

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
     * Depending on the baud-rate selected and the module type (full/half duplex) will determine the minimum
     * supported packet interval.
     * @return the minimum interval between packets supported by the current configuration.
     */
    virtual int getMinPacketInterval() const { return 1; }

    /**
     * @brief Called to indicate to the protocol that a packet has just been sent over-the-air
     * This is used to synchronise the packets from the handset to the OTA protocol to minimise latency
     */
    virtual void JustSentRFpacket() {}

    /**
     * @brief Run any RcChannelsOverride callback
     *        Should be called by incoming ChannelData generators before RCDataReceived()
     */
    void PerformChannelOverrides(uint32_t channels[], size_t channelCount)
    {
        if (RcChannelsOverrideCallback)
            RcChannelsOverrideCallback(channels, channelCount);
    }

    /**
     * Inform the handset that a valid RC packet has been received
     */
    void RCDataReceived(uint32_t channels[], size_t channelCount)
    {
        RCdataLastRecv = micros();
        for (unsigned ch=0; ch<channelCount; ++ch)
            ChannelData[ch] = channels[ch];
        if (RCdataCallback)
            RCdataCallback();
    }

    /**
     * @return the time in microseconds when the last RC packet was received from the handset
     */
    uint32_t GetRCdataLastRecv() const { return RCdataLastRecv; }

    /**
     * Set the "armed" state of the module.
     * @param armed true if the module is in the "armed" state
     */
    void SetArmed(const bool armed) { moduleArmed = armed; }

    /**
     * @return true if the protocol detects that the arming state is active
     */
    bool IsArmed() const { return moduleArmed; }

#if defined(DEBUG_TX_FREERUN)
    /**
     * @brief Can be used to force a connected callback for debugging
     */
    void forceConnection() { if (connected) connected(); }
#endif

protected:
    virtual ~Handset() = default;

    bool controllerConnected = false;
    RcChannelsOverrideCallback_fn RcChannelsOverrideCallback = nullptr;
    void (*RCdataCallback)() = nullptr;  // called when there is new RC data
    void (*disconnected)() = nullptr;    // called when RC packet stream is lost
    void (*connected)() = nullptr;       // called when RC packet stream is regained

    int32_t RequestedRCpacketInterval = 5000; // default to 200hz as per 'normal'

private:
    volatile uint32_t RCdataLastRecv = 0;
    bool moduleArmed = false;
};

#ifdef TARGET_TX
extern Handset *handset;
#endif

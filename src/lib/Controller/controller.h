#pragma once

#include "targets.h"

class Controller
{
public:
    virtual void Begin() = 0;
    virtual void End() = 0;

    void setRCDataCallback(void (*callback)()) { RCdataCallback = callback; }
    void registerParameterUpdateCallback(void (*callback)(uint8_t type, uint8_t index, uint8_t arg)) { RecvParameterUpdate = callback; }
    void registerCallbacks(void (*connectedCallback)(), void (*disconnectedCallback)(), void (*RecvModelUpdateCallback)())
    {
        connected = connectedCallback;
        disconnected = disconnectedCallback;
        RecvModelUpdate = RecvModelUpdateCallback;
    }

    virtual void handleInput() = 0;
    virtual bool IsArmed() = 0;

    virtual void setPacketInterval(int32_t PacketInterval) { RequestedRCpacketInterval = PacketInterval; }
    virtual uint8_t GetMaxPacketBytes() const { return 255; }

    virtual void JustSentRFpacket() {}
    virtual void sendTelemetryToTX(uint8_t *data) {}

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

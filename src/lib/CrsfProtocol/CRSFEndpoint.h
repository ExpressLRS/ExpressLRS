#ifndef CRSF_ENDPOINT_H
#define CRSF_ENDPOINT_H

#include "CRSFConnector.h"
#include "crc.h"
#include "lua.h"
#include "msp.h"

#include <vector>

class CRSFEndpoint {
public:
    explicit CRSFEndpoint(const crsf_addr_e device_id)
        : device_id(device_id) {}

    virtual ~CRSFEndpoint() = default;

    /**
     * Retrieves the unique device identifier assigned to this CRSF endpoint.
     *
     * @return The device ID as a value of the crsf_addr_e enumeration.
     */
    crsf_addr_e getDeviceId() const { return device_id; }

    /**
     * Handles a raw message and determines if the message should be further processed or ignored.
     *
     * This function exists for messages which don't necessarily conform to the CRSF specification.
     *
     * All messages, regardless of routing information, are passed to this function.
     *
     * @param message Pointer to the CRSF message header structure containing the message data.
     * @return A boolean value indicating whether the message should be ignored and not processed further.
     */
    virtual bool handleRaw(const crsf_header_t * message) { return false; }

    /**
     * Handles a CRSF message and determines if it should be processed further or ignored.
     *
     * This function is responsible for processing messages with the CRSF header
     * and implementing specific logic for derived classes.
     *
     * Only extended messages that are destined for this endpoint, either matching the device_id
     * or by matching the broadcast address, will be passed to this function.
     *
     * @param message Pointer to the CRSF message header structure containing the message data.
     */
    virtual void handleMessage(const crsf_header_t * message) = 0;

    void luaRegisterDevicePingCallback(void (*callback)());

    virtual void registerParameters() {}
    virtual void updateParameters() {}
    void registerLUAParameter(void *definition, luaCallback callback = nullptr, uint8_t parent = 0);
    void sendLuaCommandResponse(struct luaItem_command *cmd, luaCmdStep_e step, const char *message);
    void luaParamUpdateReq(crsf_addr_e origin, bool isElrs, uint8_t parameterType, uint8_t parameterIndex, uint8_t parameterChunk);

protected:
    void sendLuaDevicePacket(crsf_addr_e device_id);

private:
    crsf_addr_e device_id;

    void (*devicePingCallback)() = nullptr;
};

#endif //CRSF_ENDPOINT_H

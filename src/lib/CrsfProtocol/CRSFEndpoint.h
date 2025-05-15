#ifndef CRSF_ENDPOINT_H
#define CRSF_ENDPOINT_H

#include "CRSFConnector.h"
#include "CRSFParameters.h"
#include "crc.h"
#include "msp.h"

#include <vector>

#define LUA_MAX_PARAMS 64

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

    /**
     * Registers parameters for the device or system.
     *
     * It is intended that implementations of this class call the registerParameter for each parameter
     * that they wish to expose over the CRSF network.
     */
    virtual void registerParameters() {}

    /**
     * Updates parameters for the device or system.
     *
     * An implementing class would call the set...Value methods to update the parameters current value.
     */
    virtual void updateParameters() {}

protected:
    static void setLuaTextSelectionValue(selectionParameter *luaStruct, const uint8_t newValue) { luaStruct->value = newValue; }
    static void setLuaUint8Value(int8Parameter *luaStruct, const uint8_t newValue) { luaStruct->properties.u.value = newValue; }
    static void setLuaInt8Value(int8Parameter *luaStruct, const int8_t newValue) { luaStruct->properties.s.value = newValue; }
    static void setLuaUint16Value(int16Parameter *luaStruct, const uint16_t newValue) { luaStruct->properties.u.value = htobe16(newValue); }
    static void setLuaInt16Value(int16Parameter *luaStruct, const int16_t newValue) { luaStruct->properties.u.value = htobe16((uint16_t)newValue); }
    static void setLuaFloatValue(floatParameter *luaStruct, const int32_t newValue) { luaStruct->properties.value = htobe32((uint32_t)newValue); }
    static void setLuaStringValue(stringParameter *luaStruct, const char *newValue) { luaStruct->value = newValue; }

    /**
     * Invoked when a device ping message is received from the CRSF network.
     *
     * It can be overridden by derived classes to implement specific behavior upon receiving
     * a ping request. It will be called immediately before the sendDeviceInformationPacket() function.
     */
    virtual void devicePingCalled() {}
    void registerParameter(void *definition, const parameterHandlerCallback &callback = nullptr, uint8_t parent = 0);
    void sendDeviceInformationPacket();
    void parameterUpdateReq(crsf_addr_e origin, bool isElrs, uint8_t parameterType, uint8_t parameterIndex, uint8_t parameterChunk);
    void sendCommandResponse(commandParameter *cmd, commandStep_e step, const char *message);

private:
    crsf_addr_e device_id;

    // CRSF Parameter handling
    crsf_addr_e requestOrigin = CRSF_ADDRESS_BROADCAST;

    propertiesCommon *paramDefinitions[LUA_MAX_PARAMS] {};
    parameterHandlerCallback paramCallbacks[LUA_MAX_PARAMS] = {};

    uint8_t lastLuaField = 0;
    uint8_t nextStatusChunk = 0;

    static uint8_t *textSelectionParameterToArray(const selectionParameter *parameter, uint8_t *next);
    static uint8_t *commandParameterToArray(const commandParameter *parameter, uint8_t *next);
    static uint8_t *int8ParameterToArray(const int8Parameter *parameter, uint8_t *next);
    static uint8_t *int16ParameterToArray(const int16Parameter *parameter, uint8_t *next);
    static uint8_t *stringParameterToArray(const stringParameter *parameter, uint8_t *next);
    uint8_t *folderParameterToArray(const folderParameter *parameter, uint8_t *next) const;

    uint8_t sendParameter(crsf_addr_e origin, bool isElrs, crsf_frame_type_e frameType, uint8_t fieldChunk, const propertiesCommon *luaData);
    void pushResponseChunk(commandParameter *cmd, bool isElrs);
};

#endif //CRSF_ENDPOINT_H

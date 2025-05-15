#ifndef CRSF_ENDPOINT_H
#define CRSF_ENDPOINT_H

#include "CRSFParameters.h"

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
    /**
     * Invoked when a device ping message is received from the CRSF network.
     *
     * It can be overridden by derived classes to implement specific behavior upon receiving
     * a ping request. It will be called immediately before the sendDeviceInformationPacket() function.
     */
    virtual void devicePingCalled() {}

    /**
     * Registers a parameter with the CRSF endpoint.
     * 
     * @param definition Pointer to the parameter definition
     * @param callback Optional callback function to be called when the parameter is updated
     * @param parent Parent parameter index, defaults to 0 for root level parameters
     */
    void registerParameter(void *definition, const parameterHandlerCallback &callback = nullptr, uint8_t parent = 0);

    /**
     * Sends device information to the CRSF network.
     * Called after receiving a device ping message.
     */
    void sendDeviceInformationPacket();

    /**
     * Handles parameter update requests from the CRSF network.
     * 
     * @param origin The address of the requesting device
     * @param isElrs Boolean indicating if this is an ELRS-specific parameter
     * @param parameterType The type of parameter being updated
     * @param parameterIndex The index of the parameter to update
     * @param parameterChunk The chunk number for multi-part parameters
     */
    void parameterUpdateReq(crsf_addr_e origin, bool isElrs, uint8_t parameterType, uint8_t parameterIndex, uint8_t parameterChunk);

    /**
     * Sends a command response back to the CRSF network.
     * 
     * @param cmd Pointer to the command parameter structure
     * @param step The current step in the command execution
     * @param message Response message to send
     */
    void sendCommandResponse(commandParameter *cmd, commandStep_e step, const char *message);

    /**
     * Sets the value of a text selection parameter in the CRSF interface.
     *
     * @param luaStruct Pointer to the selection parameter structure to modify
     * @param newValue The new value to set
     */
    static void setTextSelectionValue(selectionParameter *luaStruct, const uint8_t newValue) { luaStruct->value = newValue; }

    /**
     * Sets an unsigned 8-bit integer value in a CRSF parameter structure.
     *
     * @param luaStruct Pointer to the int8Parameter structure to modify
     * @param newValue The new unsigned 8-bit value to set
     */
    static void setUint8Value(int8Parameter *luaStruct, const uint8_t newValue) { luaStruct->properties.u.value = newValue; }

    /**
     * Sets a signed 8-bit integer value in a CRSF parameter structure.
     *
     * @param luaStruct Pointer to the int8Parameter structure to modify
     * @param newValue The new signed 8-bit value to set
     */
    static void setInt8Value(int8Parameter *luaStruct, const int8_t newValue) { luaStruct->properties.s.value = newValue; }

    /**
     * Sets an unsigned 16-bit integer value in a CRSF parameter structure.
     * Handles endianness conversion to big-endian.
     *
     * @param luaStruct Pointer to the int16Parameter structure to modify
     * @param newValue The new unsigned 16-bit value to set
     */
    static void setUint16Value(int16Parameter *luaStruct, const uint16_t newValue) { luaStruct->properties.u.value = htobe16(newValue); }

    /**
     * Sets a signed 16-bit integer value in a CRSF parameter structure.
     * Handles endianness conversion to big-endian.
     *
     * @param luaStruct Pointer to the int16Parameter structure to modify
     * @param newValue The new signed 16-bit value to set
     */
    static void setInt16Value(int16Parameter *luaStruct, const int16_t newValue) { luaStruct->properties.u.value = htobe16((uint16_t)newValue); }

    /**
     * Sets a float value in a CRSF parameter structure.
     * Handles endianness conversion to big-endian.
     *
     * @param luaStruct Pointer to the floatParameter structure to modify
     * @param newValue The new value to set as a 32-bit integer
     */
    static void setFloatValue(floatParameter *luaStruct, const int32_t newValue) { luaStruct->properties.value = htobe32((uint32_t)newValue); }

    /**
     * Sets a string value in a CRSF parameter structure.
     *
     * @param luaStruct Pointer to the stringParameter structure to modify
     * @param newValue The new string value to set
     */
    static void setStringValue(stringParameter *luaStruct, const char *newValue) { luaStruct->value = newValue; }

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
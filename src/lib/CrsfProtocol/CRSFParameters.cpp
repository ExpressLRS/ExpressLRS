#include "CRSFRouter.h"

#include "CRSFParameters.h"

#include "logging.h"
#include "options.h"

// LUA VARIABLES//

static folderParameter paramRootFolder = {
    .common = {
        .name = "HooJ",
        .type = CRSF_FOLDER,
        .id = 0,
        .parent = 0,
    }
};

static uint8_t selectionOptionMax(const char *strOptions)
{
    // Returns the max index of the semicolon-delimited option string
    // e.g. A;B;C;D = 3
    uint8_t retVal = 0;
    while (true)
    {
        const char c = *strOptions++;
        if (c == ';')
        {
            ++retVal;
        }
        else if (c == '\0')
        {
            return retVal;
        }
    }
}

static uint8_t getSelectionLabelLength(const char *text)
{
    auto c = text;
    // get label length up to null or selection separator ';'
    while (*c != ';' && *c != '\0')
    {
        c++;
    }
    return c - text;
}

/**
 * @brief Finds a label corresponding to a specific value from a selectionParameter structure and writes the result
 *        into a destination array, appending units if applicable.
 *
 * The method iterates through the options in the given selectionParameter structure, locating the selection
 * based on the provided value. Once located, the label and units corresponding to the selection are written
 * into the output array.
 *
 * @param parameter Pointer to a `selectionParameter` structure containing the options and units.
 * @param outArray Pointer to an output array where the result label and units will be written.
 *                 Ensure that the output array is adequately sized to hold the result.
 * @param value Index of the desired label within the options of the `selectionParameter` structure.
 *
 * @return The length of the string written to the output array, or 0 if no corresponding label is found.
 */
uint8_t findSelectionLabel(const selectionParameter *parameter, char *outArray, const uint8_t value)
{
    auto c = (char *)parameter->options;
    uint8_t count = 0;
    while (*c != '\0')
    {
        // if count is equal to the parameter value, print out the label to the array
        if (count == value)
        {
            uint8_t labelLength = getSelectionLabelLength(c);
            // write the label to the destination array
            strlcpy(outArray, c, labelLength + 1);
            strlcpy(outArray + labelLength, parameter->units, strlen(parameter->units) + 1);
            return strlen(outArray);
        }
        // increment the count until value is found
        if (*c == ';')
        {
            count++;
        }
        c++;
    }
    return 0;
}

uint8_t *CRSFEndpoint::textSelectionParameterToArray(const selectionParameter *parameter, uint8_t *next)
{
    next = (uint8_t *)stpcpy((char *)next, parameter->options) + 1;
    *next++ = parameter->value;                          // value
    *next++ = 0;                                  // min
    *next++ = selectionOptionMax(parameter->options); // max
    *next++ = 0;                                  // default value
    return (uint8_t *)stpcpy((char *)next, parameter->units);
}

uint8_t *CRSFEndpoint::commandParameterToArray(const commandParameter *parameter, uint8_t *next)
{
    *next++ = parameter->step;
    *next++ = 200; // timeout in 10ms
    return (uint8_t *)stpcpy((char *)next, parameter->info);
}

uint8_t *CRSFEndpoint::int8ParameterToArray(const int8Parameter *parameter, uint8_t *next)
{
    memcpy(next, &parameter->properties, sizeof(parameter->properties));
    next += sizeof(parameter->properties);
    *next++ = 0; // default value
    return (uint8_t *)stpcpy((char *)next, parameter->units);
}

uint8_t *CRSFEndpoint::int16ParameterToArray(const int16Parameter *parameter, uint8_t *next)
{
    memcpy(next, &parameter->properties, sizeof(parameter->properties));
    next += sizeof(parameter->properties);
    *next++ = 0; // default value byte 1
    *next++ = 0; // default value byte 2
    return (uint8_t *)stpcpy((char *)next, parameter->units);
}

uint8_t *CRSFEndpoint::stringParameterToArray(const stringParameter *parameter, uint8_t *next)
{
    return (uint8_t *)stpcpy((char *)next, parameter->value);
}

/**
 * @brief Converts a folder parameter into a formatted array representation, appending child parameter indices.
 *
 * This function takes a `folderParameter` structure, copies its name (dynamic or static) into the provided array,
 * and appends the indices of its child parameters based on the `paramDefinitions` list. Child parameter indices are
 * appended to the array and terminated with an end-of-list `0xFF` marker.
 *
 * @param parameter Pointer to the `folderParameter` structure containing the parameter details, including its name and identifier.
 * @param next Pointer to the output array where the folder parameter name and child parameter indices will be stored.
 *             Ensure the array has adequate space to accommodate the data being written.
 *
 * @return Pointer to the end of the written data within the provided output array.
 */
uint8_t *CRSFEndpoint::folderParameterToArray(const folderParameter *parameter, uint8_t *next) const
{
    auto childParameters = (uint8_t *)stpcpy((char *)next, parameter->dyn_name ? parameter->dyn_name : parameter->common.name) + 1;
    for (int i = 1; i <= lastLuaField; i++)
    {
        if (paramDefinitions[i]->parent == parameter->common.id)
        {
            *childParameters++ = i;
        }
    }
    *childParameters = 0xFF;
    return childParameters;
}

uint8_t CRSFEndpoint::sendParameter(const crsf_addr_e origin, const bool isElrs, const crsf_frame_type_e frameType, const uint8_t fieldChunk, const propertiesCommon *luaData)
{
    const uint8_t dataType = luaData->type & CRSF_FIELD_TYPE_MASK;

    // 256 max payload + (FieldID + ChunksRemain + Parent + Type)
    // Chunk 1: (FieldID + ChunksRemain + Parent + Type) + fieldChunk0 data
    // Chunk 2-N: (FieldID + ChunksRemain) + fieldChunk1 data
    uint8_t chunkBuffer[256 + 4];
    // Start the field payload at 2 to leave room for (FieldID + ChunksRemain)
    chunkBuffer[2] = luaData->parent;
    chunkBuffer[3] = dataType;
    // Set the hidden flag
    chunkBuffer[3] |= luaData->type & CRSF_FIELD_HIDDEN ? 0x80 : 0;
    if (isElrs)
    {
        chunkBuffer[3] |= luaData->type & CRSF_FIELD_ELRS_HIDDEN ? 0x80 : 0;
    }
    uint8_t paramInformation[CRSF_MAX_PACKET_LEN];

    // Copy the name to the buffer starting at chunkBuffer[4]
    uint8_t *chunkStart = (uint8_t *)stpcpy((char *)&chunkBuffer[4], luaData->name) + 1;
    uint8_t *dataEnd;

    switch (dataType)
    {
    case CRSF_TEXT_SELECTION:
        dataEnd = textSelectionParameterToArray((selectionParameter *)luaData, chunkStart);
        break;
    case CRSF_COMMAND:
        dataEnd = commandParameterToArray((commandParameter *)luaData, chunkStart);
        break;
    case CRSF_INT8: // fallthrough
    case CRSF_UINT8:
        dataEnd = int8ParameterToArray((int8Parameter *)luaData, chunkStart);
        break;
    case CRSF_INT16: // fallthrough
    case CRSF_UINT16:
        dataEnd = int16ParameterToArray((int16Parameter *)luaData, chunkStart);
        break;
    case CRSF_STRING: // fallthrough
    case CRSF_INFO:
        dataEnd = stringParameterToArray((stringParameter *)luaData, chunkStart);
        break;
    case CRSF_FOLDER:
        // re-fetch the lua data name, because luaFolderStructToArray will decide whether
        // to return the fixed name or dynamic name.
        dataEnd = folderParameterToArray((folderParameter *)luaData, &chunkBuffer[4]);
        break;
    case CRSF_FLOAT:
    case CRSF_OUT_OF_RANGE:
    default:
        return 0;
    }

    // dataEnd points to the end of the last string
    // -2 bytes Lua chunk header: FieldId, ChunksRemain
    // +1 for the null on the last string
    const uint8_t dataSize = (dataEnd - chunkBuffer) - 2 + 1;
    // Maximum number of chunked bytes that can be sent in one response
    // 6 bytes CRSF header/CRC: Dest, Len, Type, ExtSrc, ExtDst, CRC
    // 2 bytes Lua chunk header: FieldId, ChunksRemain
    // Ask the endpoint what the maximum size for the packet based on the origin;
    // this is for slow baud-rates to the handset
    const uint8_t chunkMax = crsfRouter.getConnectorMaxPacketSize(origin) - 6 - 2;
    // How many chunks needed to send this field (rounded up)
    const uint8_t chunkCnt = (dataSize + chunkMax - 1) / chunkMax;
    // Data left to send is adjustedSize - chunks sent already
    const uint8_t chunkSize = std::min((uint8_t)(dataSize - (fieldChunk * chunkMax)), chunkMax);

    // Move chunkStart back 2 bytes to add (FieldId + ChunksRemain) to each packet
    chunkStart = &chunkBuffer[fieldChunk * chunkMax];
    chunkStart[0] = luaData->id;                 // FieldId
    chunkStart[1] = chunkCnt - (fieldChunk + 1); // ChunksRemain
    memcpy(paramInformation + sizeof(crsf_ext_header_t), chunkStart, chunkSize + 2);
    crsfRouter.SetExtendedHeaderAndCrc((crsf_ext_header_t *)paramInformation, frameType, CRSF_EXT_FRAME_SIZE(chunkSize + 2), origin, device_id);
    crsfRouter.deliverMessage(nullptr, (crsf_header_t *)paramInformation);
    return chunkCnt - (fieldChunk + 1);
}

void CRSFEndpoint::pushResponseChunk(commandParameter *cmd, const bool isElrs)
{
    DBGVLN("sending response for [%s] chunk=%u step=%u", cmd->common.name, nextStatusChunk, cmd->step);
    if (sendParameter(requestOrigin, isElrs, CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, nextStatusChunk, (propertiesCommon *)cmd) == 0)
    {
        nextStatusChunk = 0;
    }
    else
    {
        nextStatusChunk++;
    }
}

void CRSFEndpoint::sendCommandResponse(commandParameter *cmd, const commandStep_e step, const char *message)
{
    cmd->step = step;
    cmd->info = message;
    nextStatusChunk = 0;
    pushResponseChunk(cmd, false);
}

void CRSFEndpoint::registerParameter(void *definition, const parameterHandlerCallback &callback, const uint8_t parent)
{
    // On the first call we initialise the root folder
    if (lastLuaField == 0)
    {
        paramDefinitions[0] = (propertiesCommon *)&paramRootFolder;
        paramCallbacks[0] = nullptr;
    }
    // Add the new parameter definition to the list
    const auto p = (propertiesCommon *)definition;
    lastLuaField++;
    p->id = lastLuaField;
    p->parent = parent;
    paramDefinitions[lastLuaField] = p;
    paramCallbacks[lastLuaField] = callback;
}

void CRSFEndpoint::parameterUpdateReq(const crsf_addr_e origin, const bool isElrs, const uint8_t parameterType, const uint8_t parameterIndex, const uint8_t parameterChunk)
{
    propertiesCommon *parameter = paramDefinitions[parameterIndex];
    requestOrigin = origin;

    switch (parameterType)
    {
    case CRSF_FRAMETYPE_PARAMETER_WRITE:
        DBGLN("Set Lua [%s]=%u", parameter->name, parameterChunk);
        if (parameterIndex < LUA_MAX_PARAMS && paramCallbacks[parameterIndex])
        {
            // While the command is executing, the handset will send `WRITE state=lcsQuery`.
            // paramCallbacks will set the value when nextStatusChunk == 0, or send any
            // remaining chunks when nextStatusChunk != 0
            if (parameterChunk == lcsQuery && nextStatusChunk != 0)
            {
                pushResponseChunk((commandParameter *)parameter, isElrs);
            }
            else
            {
                paramCallbacks[parameterIndex](parameter, parameterChunk);
            }
        }
        break;

    case CRSF_FRAMETYPE_DEVICE_PING:
        devicePingCalled();
        sendDeviceInformationPacket();
        break;

    case CRSF_FRAMETYPE_PARAMETER_READ: {
        DBGVLN("Read lua param %u %u", fieldId, fieldChunk);
        if (parameterIndex < LUA_MAX_PARAMS && parameter)
        {
            const auto field = (commandParameter *)parameter;
            const uint8_t dataType = field->common.type & CRSF_FIELD_TYPE_MASK;
            // On first chunk of a command, reset the step/info of the command
            if (dataType == CRSF_COMMAND && parameterChunk == 0)
            {
                field->step = lcsIdle;
                field->info = "";
            }
            sendParameter(origin, isElrs, CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, parameterChunk, &field->common);
        }
    }
    break;

    default:
        DBGLN("Unknown LUA %x", parameterType);
    }
}

/***
 * @brief: Convert `version` (string) to a integer version representation
 * e.g. "2.2.15 ISM24G" => 0x0002020f
 * Assumes all version fields are < 256, the number portion
 * MUST be followed by a space to correctly be parsed
 ***/
uint32_t VersionStrToU32(const char *verStr)
{
    uint32_t retVal = 0;
#if !defined(FORCE_NO_DEVICE_VERSION)
    uint8_t accumulator = 0;
    char c;
    bool trailing_data = false;
    while ((c = *verStr))
    {
        ++verStr;
        // A decimal indicates moving to a new version field
        if (c == '.')
        {
            retVal = (retVal << 8) | accumulator;
            accumulator = 0;
            trailing_data = false;
        }
        // Else if this is a number add it up
        else if (c >= '0' && c <= '9')
        {
            accumulator = (accumulator * 10) + (c - '0');
            trailing_data = true;
        }
        // Anything except [0-9.] ends the parsing
        else
        {
            break;
        }
    }
    if (trailing_data)
    {
        retVal = (retVal << 8) | accumulator;
    }
    // If the version ID is < 1.0.0, we could not parse it,
    // just use the OTA version as the major version number
    if (retVal < 0x010000)
    {
        retVal = OTA_VERSION_ID << 16;
    }
#endif
    return retVal;
}

void CRSFEndpoint::sendDeviceInformationPacket()
{
    uint8_t deviceInformation[DEVICE_INFORMATION_LENGTH];
    const uint8_t size = strlen(device_name) + 1;
    auto *device = (deviceInformationPacket_t *)(deviceInformation + sizeof(crsf_ext_header_t) + size);
    // Packet starts with device name
    memcpy(deviceInformation + sizeof(crsf_ext_header_t), device_name, size);
    // Followed by the device
    device->serialNo = htobe32(0x454C5253);                  // ['E', 'L', 'R', 'S'], seen [0x00, 0x0a, 0xe7, 0xc6] // "Serial 177-714694" (value is 714694)
    device->hardwareVer = 0;                                 // unused currently by us, seen [ 0x00, 0x0b, 0x10, 0x01 ] // "Hardware: V 1.01" / "Bootloader: V 3.06"
    device->softwareVer = htobe32(VersionStrToU32(version)); // seen [ 0x00, 0x00, 0x05, 0x0f ] // "Firmware: V 5.15"
    device->fieldCnt = lastLuaField;
    device->parameterVersion = 0;
    crsfRouter.SetExtendedHeaderAndCrc((crsf_ext_header_t *)deviceInformation, CRSF_FRAMETYPE_DEVICE_INFO, DEVICE_INFORMATION_FRAME_SIZE, requestOrigin, device_id);
    crsfRouter.deliverMessage(nullptr, (crsf_header_t *)deviceInformation);
}

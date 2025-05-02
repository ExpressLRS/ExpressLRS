#include "lua.h"
#include "CRSFRouter.h"
#include "common.h"
#include "logging.h"
#include "options.h"

#ifdef TARGET_RX
#include "telemetry.h"
#else
#include "CRSFHandset.h"
#endif
static void (*devicePingCallback)() = nullptr;

//LUA VARIABLES//

#define LUA_MAX_PARAMS 64
static crsf_addr_e requestOrigin;

static luaItem_folder luaAgentLite = {
    .common = {
        .name = "HooJ",
        .type = CRSF_FOLDER,
        .id = 0,
        .parent = 0
      },
  };

static luaPropertiesCommon *paramDefinitions[LUA_MAX_PARAMS] = {(luaPropertiesCommon *)&luaAgentLite, nullptr}; // array of luaItem_*
static luaCallback paramCallbacks[LUA_MAX_PARAMS] = {nullptr};

static uint8_t lastLuaField = 0;
static uint8_t nextStatusChunk = 0;

static uint8_t luaSelectionOptionMax(const char *strOptions)
{
  // Returns the max index of the semicolon-delimited option string
  // e.g. A;B;C;D = 3
  uint8_t retVal = 0;
  while (true)
  {
    char c = *strOptions++;
    if (c == ';')
      ++retVal;
    else if (c == '\0')
      return retVal;
  }
}

uint8_t getLabelLength(char *text, char separator){
  char *c = (char*)text;
  //get label length up to null or lua separator ;
  while(*c != separator && *c != '\0'){
    c++;
  }
  return c-text;
}

uint8_t findLuaSelectionLabel(const void *luaStruct, char *outarray, uint8_t value)
{
  const struct luaItem_selection *p1 = (const struct luaItem_selection *)luaStruct;
  char *c = (char *)p1->options;
  uint8_t count = 0;
  while (*c != '\0'){
    //if count is equal to the parameter value, print out the label to the array
    if(count == value){
      uint8_t labelLength = getLabelLength(c,';');
      //write label to destination array
      strlcpy(outarray, c, labelLength+1);
      strlcpy(outarray + labelLength, p1->units, strlen(p1->units)+1);
      return strlen(outarray);
    }
    //increment the count until value is found
    if(*c == ';'){
      count++;
    }
    c++;
  }
  return 0;
}

static uint8_t *luaTextSelectionStructToArray(const void *luaStruct, uint8_t *next)
{
  const struct luaItem_selection *p1 = (const struct luaItem_selection *)luaStruct;
  next = (uint8_t *)stpcpy((char *)next, p1->options) + 1;
  *next++ = p1->value; // value
  *next++ = 0; // min
  *next++ = luaSelectionOptionMax(p1->options); //max
  *next++ = 0; // default value
  return (uint8_t *)stpcpy((char *)next, p1->units);
}

static uint8_t *luaCommandStructToArray(const void *luaStruct, uint8_t *next)
{
  const struct luaItem_command *p1 = (const struct luaItem_command *)luaStruct;
  *next++ = p1->step;
  *next++ = 200; // timeout in 10ms
  return (uint8_t *)stpcpy((char *)next, p1->info);
}

static uint8_t *luaInt8StructToArray(const void *luaStruct, uint8_t *next)
{
  const struct luaItem_int8 *p1 = (const struct luaItem_int8 *)luaStruct;
  memcpy(next, &p1->properties, sizeof(p1->properties));
  next += sizeof(p1->properties);
  *next++ = 0; // default value
  return (uint8_t *)stpcpy((char *)next, p1->units);
}

static uint8_t *luaInt16StructToArray(const void *luaStruct, uint8_t *next)
{
  const struct luaItem_int16 *p1 = (const struct luaItem_int16 *)luaStruct;
  memcpy(next, &p1->properties, sizeof(p1->properties));
  next += sizeof(p1->properties);
  *next++ = 0; // default value byte 1
  *next++ = 0; // default value byte 2
  return (uint8_t *)stpcpy((char *)next, p1->units);
}

static uint8_t *luaStringStructToArray(const void *luaStruct, uint8_t *next)
{
  const struct luaItem_string *p1 = (const struct luaItem_string *)luaStruct;
  return (uint8_t *)stpcpy((char *)next, p1->value);
}

static uint8_t *luaFolderStructToArray(const void *luaStruct, uint8_t *next)
{
  const struct luaItem_folder *p1 = (const struct luaItem_folder *)luaStruct;
  uint8_t *childParameters;
  if(p1->dyn_name != NULL){
    childParameters = (uint8_t *)stpcpy((char *)next, p1->dyn_name) + 1;
  } else {
    childParameters = (uint8_t *)stpcpy((char *)next, p1->common.name) + 1;
  }
  for (int i=1;i<=lastLuaField;i++)
  {
    if (paramDefinitions[i]->parent == p1->common.id)
    {
      *childParameters++ = i;
    }
  }
  *childParameters = 0xFF;
  return childParameters;
}

/***
 * @brief: Turn a lua param structure into a chunk of CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY frame and queue it
 * @returns: Number of chunks left to send after this one
 */
static uint8_t sendCRSFparam(crsf_addr_e origin, bool isElrs, uint8_t fieldChunk, struct luaPropertiesCommon *luaData)
{
  uint8_t dataType = luaData->type & CRSF_FIELD_TYPE_MASK;

  // 256 max payload + (FieldID + ChunksRemain + Parent + Type)
  // Chunk 1: (FieldID + ChunksRemain + Parent + Type) + fieldChunk0 data
  // Chunk 2-N: (FieldID + ChunksRemain) + fieldChunk1 data
  uint8_t chunkBuffer[256+4];
  // Start the field payload at 2 to leave room for (FieldID + ChunksRemain)
  chunkBuffer[2] = luaData->parent;
  chunkBuffer[3] = dataType;
  // Set the hidden flag
  chunkBuffer[3] |= luaData->type & CRSF_FIELD_HIDDEN ? 0x80 : 0;
  if (isElrs) {
    chunkBuffer[3] |= luaData->type & CRSF_FIELD_ELRS_HIDDEN ? 0x80 : 0;
  }
  uint8_t paramInformation[CRSF_MAX_PACKET_LEN];

  // Copy the name to the buffer starting at chunkBuffer[4]
  uint8_t *chunkStart = (uint8_t *)stpcpy((char *)&chunkBuffer[4], luaData->name) + 1;
  uint8_t *dataEnd;

  switch(dataType) {
    case CRSF_TEXT_SELECTION:
      dataEnd = luaTextSelectionStructToArray(luaData, chunkStart);
      break;
    case CRSF_COMMAND:
      dataEnd = luaCommandStructToArray(luaData, chunkStart);
      break;
    case CRSF_INT8: // fallthrough
    case CRSF_UINT8:
      dataEnd = luaInt8StructToArray(luaData, chunkStart);
      break;
    case CRSF_INT16: // fallthrough
    case CRSF_UINT16:
      dataEnd = luaInt16StructToArray(luaData, chunkStart);
      break;
    case CRSF_STRING: // fallthrough
    case CRSF_INFO:
      dataEnd = luaStringStructToArray(luaData, chunkStart);
      break;
    case CRSF_FOLDER:
      // re-fetch the lua data name, because luaFolderStructToArray will decide whether
      // to return the fixed name or dynamic name.
      dataEnd = luaFolderStructToArray(luaData, &chunkBuffer[4]);
      break;
    case CRSF_FLOAT:
    case CRSF_OUT_OF_RANGE:
    default:
      return 0;
  }

  // dataEnd points to the end of the last string
  // -2 bytes Lua chunk header: FieldId, ChunksRemain
  // +1 for the null on the last string
  uint8_t dataSize = (dataEnd - chunkBuffer) - 2 + 1;
  // Maximum number of chunked bytes that can be sent in one response
  // 6 bytes CRSF header/CRC: Dest, Len, Type, ExtSrc, ExtDst, CRC
  // 2 bytes Lua chunk header: FieldId, ChunksRemain
#ifdef TARGET_TX
  uint8_t chunkMax = handset->GetMaxPacketBytes() - 6 - 2;
#else
  uint8_t chunkMax = CRSF_MAX_PACKET_LEN - 6 - 2;
#endif
  // How many chunks needed to send this field (rounded up)
  uint8_t chunkCnt = (dataSize + chunkMax - 1) / chunkMax;
  // Data left to send is adjustedSize - chunks sent already
  uint8_t chunkSize = std::min((uint8_t)(dataSize - (fieldChunk * chunkMax)), chunkMax);

  // Move chunkStart back 2 bytes to add (FieldId + ChunksRemain) to each packet
  chunkStart = &chunkBuffer[fieldChunk * chunkMax];
  chunkStart[0] = luaData->id;                 // FieldId
  chunkStart[1] = chunkCnt - (fieldChunk + 1); // ChunksRemain
  memcpy(paramInformation + sizeof(crsf_ext_header_t), chunkStart, chunkSize + 2);
#if defined(TARGET_TX)
  crsfRouter.SetExtendedHeaderAndCrc((crsf_ext_header_t *)paramInformation, CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, CRSF_EXT_FRAME_SIZE(chunkSize + 2), origin, CRSF_ADDRESS_CRSF_TRANSMITTER);
#else
  crsfRouter.SetExtendedHeaderAndCrc((crsf_ext_header_t *)paramInformation, CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, CRSF_EXT_FRAME_SIZE(chunkSize + 2), origin, CRSF_ADDRESS_CRSF_RECEIVER);
#endif
  crsfRouter.deliverMessage(nullptr, (crsf_header_t *)paramInformation);
  return chunkCnt - (fieldChunk+1);
}

static void pushResponseChunk(struct luaItem_command *cmd, bool isElrs) {
  DBGVLN("sending response for [%s] chunk=%u step=%u", cmd->common.name, nextStatusChunk, cmd->step);
  if (sendCRSFparam(requestOrigin, isElrs, nextStatusChunk, (struct luaPropertiesCommon *)cmd) == 0) {
    nextStatusChunk = 0;
  } else {
    nextStatusChunk++;
  }
}

void sendLuaCommandResponse(struct luaItem_command *cmd, luaCmdStep_e step, const char *message) {
  cmd->step = step;
  cmd->info = message;
  nextStatusChunk = 0;
  pushResponseChunk(cmd, false);
}

void luaRegisterDevicePingCallback(void (*callback)())
{
  devicePingCallback = callback;
}

void registerLUAParameter(void *definition, luaCallback callback, uint8_t parent)
{
  luaPropertiesCommon *p = (struct luaPropertiesCommon *)definition;
  lastLuaField++;
  p->id = lastLuaField;
  p->parent = parent;
  paramDefinitions[lastLuaField] = p;
  paramCallbacks[lastLuaField] = callback;
}

void luaParamUpdateReq(crsf_addr_e origin, bool isElrs, uint8_t parameterType, uint8_t parameterIndex, uint8_t parameterChunk)
{
  struct luaPropertiesCommon *parameter = paramDefinitions[parameterIndex];
  requestOrigin = origin;

  switch(parameterType)
  {
    case CRSF_FRAMETYPE_PARAMETER_WRITE:
      DBGLN("Set Lua [%s]=%u", parameter->name, parameterChunk);
      if (parameterIndex < LUA_MAX_PARAMS && paramCallbacks[parameterIndex]) {
        // While the command is executing, the handset will send `WRITE state=lcsQuery`.
        // paramCallbacks will set the value when nextStatusChunk == 0, or send any
        // remaining chunks when nextStatusChunk != 0
        if (parameterChunk == lcsQuery && nextStatusChunk != 0) {
          pushResponseChunk((struct luaItem_command *)parameter, isElrs);
        } else {
          paramCallbacks[parameterIndex](parameter, parameterChunk);
        }
      }
      break;

    case CRSF_FRAMETYPE_DEVICE_PING:
        devicePingCallback();
#ifdef TARGET_TX
        sendLuaDevicePacket(CRSF_ADDRESS_CRSF_TRANSMITTER);
#else
        sendLuaDevicePacket(CRSF_ADDRESS_CRSF_RECEIVER);
#endif
        break;

    case CRSF_FRAMETYPE_PARAMETER_READ:
      {
        DBGVLN("Read lua param %u %u", fieldId, fieldChunk);
        if (parameterIndex < LUA_MAX_PARAMS && parameter)
        {
          const auto field = (struct luaItem_command *)parameter;
          uint8_t dataType = field->common.type & CRSF_FIELD_TYPE_MASK;
          // On first chunk of a command, reset the step/info of the command
          if (dataType == CRSF_COMMAND && parameterChunk == 0)
          {
            field->step = lcsIdle;
            field->info = "";
          }
          sendCRSFparam(origin, isElrs, parameterChunk, &field->common);
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

void sendLuaDevicePacket(crsf_addr_e device_id)
{
  uint8_t deviceInformation[DEVICE_INFORMATION_LENGTH];
  const uint8_t size = strlen(device_name)+1;
  auto *device = (deviceInformationPacket_t *)(deviceInformation + sizeof(crsf_ext_header_t) + size);
  // Packet starts with device name
  memcpy(deviceInformation + sizeof(crsf_ext_header_t), device_name, size);
  // Followed by the device
  device->serialNo = htobe32(0x454C5253); // ['E', 'L', 'R', 'S'], seen [0x00, 0x0a, 0xe7, 0xc6] // "Serial 177-714694" (value is 714694)
  device->hardwareVer = 0; // unused currently by us, seen [ 0x00, 0x0b, 0x10, 0x01 ] // "Hardware: V 1.01" / "Bootloader: V 3.06"
  device->softwareVer = htobe32(VersionStrToU32(version)); // seen [ 0x00, 0x00, 0x05, 0x0f ] // "Firmware: V 5.15"
  device->fieldCnt = lastLuaField;
  device->parameterVersion = 0;
  crsfRouter.SetExtendedHeaderAndCrc((crsf_ext_header_t *)deviceInformation, CRSF_FRAMETYPE_DEVICE_INFO, DEVICE_INFORMATION_FRAME_SIZE, requestOrigin, device_id);
  crsfRouter.deliverMessage(nullptr, (crsf_header_t *)deviceInformation);
}

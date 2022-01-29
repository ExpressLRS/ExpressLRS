#ifdef TARGET_RX

#include "lua.h"
#include "CRSF.h"
#include "logging.h"
#include "telemetry.h"

extern CRSF crsf;

extern Telemetry telemetry;

static volatile bool UpdateParamReq = false;

//LUA VARIABLES//

#define LUA_MAX_PARAMS 32
static const void *paramDefinitions[LUA_MAX_PARAMS] = {0}; // array of luaItem_*
static luaCallback paramCallbacks[LUA_MAX_PARAMS] = {0};
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

static uint8_t sendCRSFparam(crsf_frame_type_e frameType, uint8_t fieldChunk, struct luaPropertiesCommon *luaData)
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
  chunkBuffer[3] |= luaData->type;

  // Copy the name to the buffer starting at chunkBuffer[4]
  uint8_t *chunkStart = (uint8_t *)stpcpy((char *)&chunkBuffer[4], luaData->name) + 1;
  uint8_t paramInformation[DEVICE_INFORMATION_LENGTH];

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
    case CRSF_STRING: // fallthough
    case CRSF_INFO:
      dataEnd = luaStringStructToArray(luaData, chunkStart);
      break;
    case CRSF_FOLDER:
      // Nothing to do, the name is all there is
      // but subtract 1 because dataSize expects the end to not include the null
      // which is already accounted for in chunkStart
      dataEnd = chunkStart - 1;
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
  // use device info payload length for now, as im not sure how the telemetry packet chunking yet.
  // 2 bytes Lua chunk header: FieldId, ChunksRemain
  uint8_t chunkMax = CRSF_MAX_PACKET_LEN - 6 - 2;
  // How many chunks needed to send this field (rounded up)
  uint8_t chunkCnt = (dataSize + chunkMax - 1) / chunkMax;
  // Data left to send is adjustedSize - chunks sent already
  uint8_t chunkSize = min((uint8_t)(dataSize - (fieldChunk * chunkMax)), chunkMax);

  // Move chunkStart back 2 bytes to add (FieldId + ChunksRemain) to each packet
  chunkStart = &chunkBuffer[fieldChunk * chunkMax];
  chunkStart[0] = luaData->id;                 // FieldId
  chunkStart[1] = chunkCnt - (fieldChunk + 1); // ChunksRemain
  memcpy(paramInformation + sizeof(crsf_ext_header_t),chunkStart,chunkSize + 2);
  
  crsf.SetExtendedHeaderAndCrc(paramInformation, frameType, chunkSize + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + 2, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_CRSF_TRANSMITTER);

  telemetry.AppendTelemetryPackage(paramInformation);
  
  //#CRSF::packetQueueExtended(frameType, chunkStart, chunkSize + 2);

  return chunkCnt - (fieldChunk+1);
}

static void pushResponseChunk(struct luaItem_command *cmd) {
  DBGVLN("sending response for [%s] chunk=%u step=%u", cmd->common.name, nextStatusChunk, cmd->step);
  if (sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, nextStatusChunk, (struct luaPropertiesCommon *)cmd) == 0) {
    nextStatusChunk = 0;
  } else {
    nextStatusChunk++;
  }
}

void sendLuaCommandResponse(struct luaItem_command *cmd, uint8_t step, const char *message) {
  cmd->step = step;
  cmd->info = message;
  nextStatusChunk = 0;
  pushResponseChunk(cmd);
}

void ICACHE_RAM_ATTR luaParamUpdateReq()
{
  UpdateParamReq = true;
}

void registerLUAParameter(void *definition, luaCallback callback, uint8_t parent)
{
  if (definition == NULL)
  {
    static uint8_t agentLiteFolder[4+LUA_MAX_PARAMS+2] = "HooJ";
    static struct luaItem_folder luaAgentLite = {
        {(const char *)agentLiteFolder, CRSF_FOLDER},
    };

    paramDefinitions[0] = &luaAgentLite;
    paramCallbacks[0] = 0;
    uint8_t *pos = agentLiteFolder + 4;
    for (int i=1;i<=lastLuaField;i++)
    {
      struct luaPropertiesCommon *p = (struct luaPropertiesCommon *)paramDefinitions[i];
      if (p->parent == 0) {
        *pos++ = i;
      }
    }
    *pos++ = 0xFF;
    *pos++ = 0;
    return;
  }
  struct luaPropertiesCommon *p = (struct luaPropertiesCommon *)definition;
  lastLuaField++;
  p->id = lastLuaField;
  p->parent = parent;
  paramDefinitions[p->id] = definition;
  paramCallbacks[p->id] = callback;
}

bool luaHandleUpdateParameter()
{
  if (UpdateParamReq == false)
  {
    return false;
  }

  switch(crsf.ParameterUpdateData[0])
  {
    case CRSF_FRAMETYPE_PARAMETER_WRITE:
      {
        uint8_t id = crsf.ParameterUpdateData[1];
        uint8_t arg = crsf.ParameterUpdateData[2];
        // All paramDefinitions are not luaItem_command but the common part is the same
        struct luaItem_command *p = (struct luaItem_command *)paramDefinitions[id];
        DBGLN("Set Lua [%s]=%u", p->common.name, arg);
        if (id < LUA_MAX_PARAMS && paramCallbacks[id]) {
          if (arg == 6 && nextStatusChunk != 0) {
            pushResponseChunk(p);
          } else {
            paramCallbacks[id](id, arg);
          }
        }
      }
      break;

    case CRSF_FRAMETYPE_DEVICE_PING:
      {
        sendLuaDevicePacket();
      }  
      break;

    case CRSF_FRAMETYPE_PARAMETER_READ:
      {
        uint8_t fieldId = crsf.ParameterUpdateData[1];
        uint8_t fieldChunk = crsf.ParameterUpdateData[2];
        DBGVLN("Read lua param %u %u", fieldId, fieldChunk);
        if (fieldId < LUA_MAX_PARAMS && paramDefinitions[fieldId])
        {
          struct luaItem_command *field = (struct luaItem_command *)paramDefinitions[fieldId];
          uint8_t dataType = field->common.type & CRSF_FIELD_TYPE_MASK;
          // On first chunk of a command, reset the step/info of the command
          if (dataType == CRSF_COMMAND && fieldChunk == 0)
          {
            field->step = 0;
            field->info = "";
          }
          sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, fieldChunk, &field->common);
        }
      }
      break;

    default:
    {
      DBGLN("Unknown LUA %x", crsf.ParameterUpdateData[0]);
    }
  }

  UpdateParamReq = false;
  return true;
}

void sendLuaDevicePacket(void)
{
  uint8_t deviceInformation[DEVICE_INFORMATION_LENGTH];
  crsf.GetDeviceInformation(deviceInformation, lastLuaField);
  crsf.SetExtendedHeaderAndCrc(deviceInformation, CRSF_FRAMETYPE_DEVICE_INFO, DEVICE_INFORMATION_FRAME_SIZE, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_CRSF_TRANSMITTER);
  telemetry.AppendTelemetryPackage(deviceInformation);
}

#endif

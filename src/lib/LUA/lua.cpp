#ifdef TARGET_TX

#include "lua.h"
#include "CRSF.h"
#include "logging.h"

const char txDeviceName[] = TX_DEVICE_NAME;

extern CRSF crsf;

static volatile bool UpdateParamReq = false;

//LUA VARIABLES//
#define LUA_PKTCOUNT_INTERVAL_MS 1000LU
static uint8_t luaWarningFLags = false;
static uint8_t suppressedLuaWarningFlags = true;

#define LUA_MAX_PARAMS 32
static const void *paramDefinitions[LUA_MAX_PARAMS] = {0}; // array of luaItem_*
static luaCallback paramCallbacks[LUA_MAX_PARAMS] = {0};
static void (*populateHandler)() = 0;
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
  uint8_t dataType = luaData->type & ~(CRSF_FIELD_HIDDEN|CRSF_FIELD_ELRS_HIDDEN);
  
  // 256 max payload + (FieldID + ChunksRemain + Parent + Type)
  // Chunk 1: (FieldID + ChunksRemain + Parent + Type) + fieldChunk0 data
  // Chunk 2-N: (FieldID + ChunksRemain) + fieldChunk1 data
  uint8_t chunkBuffer[256+4];
  // Start the field payload at 2 to leave room for (FieldID + ChunksRemain)
  chunkBuffer[2] = luaData->parent;
  chunkBuffer[3] = dataType;
  // Set the hidden flag
  chunkBuffer[3] |= luaData->type & CRSF_FIELD_HIDDEN ? 0x80 : 0;
  if (crsf.elrsLUAmode) {
    chunkBuffer[3] |= luaData->type & CRSF_FIELD_ELRS_HIDDEN ? 0x80 : 0;
  }
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
  // 6 bytes CRSF header/CRC: Dest, Len, Type, ExtSrc, ExtDst, CRC
  // 2 bytes Lua chunk header: FieldId, ChunksRemain
  uint8_t chunkMax = CRSF::GetMaxPacketBytes() - 6 - 2;
  // How many chunks needed to send this field (rounded up)
  uint8_t chunkCnt = (dataSize + chunkMax - 1) / chunkMax;
  // Data left to send is adjustedSize - chunks sent already
  uint8_t chunkSize = min((uint8_t)(dataSize - (fieldChunk * chunkMax)), chunkMax);

  // Move chunkStart back 2 bytes to add (FieldId + ChunksRemain) to each packet
  chunkStart = &chunkBuffer[fieldChunk * chunkMax];
  chunkStart[0] = luaData->id;                 // FieldId
  chunkStart[1] = chunkCnt - (fieldChunk + 1); // ChunksRemain
  CRSF::packetQueueExtended(frameType, chunkStart, chunkSize + 2);

  return chunkCnt - (fieldChunk+1);
}

static void pushResponseChunk(struct luaItem_command *cmd) {
  DBGVLN("sending response for [%s] chunk=%u step=%u", cmd->name, nextStatusChunk, cmd->step);
  if (sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,nextStatusChunk,(struct luaPropertiesCommon *)cmd) == 0) {
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

void suppressCurrentLuaWarning(void){ //0 to suppress
  suppressedLuaWarningFlags = ~luaWarningFLags;
}

bool getLuaWarning(void){ //1 if alarm
  return luaWarningFLags & suppressedLuaWarningFlags;
}

void sendELRSstatus()
{
  uint8_t buffer[sizeof(tagLuaElrsParams) + 0];
  struct tagLuaElrsParams * const params = (struct tagLuaElrsParams *)buffer;

  params->pktsBad = crsf.BadPktsCountResult;
  params->pktsGood = htobe16(crsf.GoodPktsCountResult);
  params->flags = getLuaWarning();
  // to support sending a params.msg, buffer should be extended by the strlen of the message
  // and copied into params->msg (with trailing null)
  params->msg[0] = '\0';

  crsf.packetQueueExtended(0x2E, &buffer, sizeof(buffer));
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

void registerLUAPopulateParams(void (*populate)())
{
  populateHandler = populate;
  populate();
}

bool luaHandleUpdateParameter()
{
  static uint32_t LUAfieldReported = 0;

  if (millis() >= (uint32_t)(LUA_PKTCOUNT_INTERVAL_MS + LUAfieldReported)){
      LUAfieldReported = millis();
      populateHandler();
      sendELRSstatus();
  }

  if (UpdateParamReq == false)
  {
    return false;
  }

  switch(crsf.ParameterUpdateData[0])
  {
    case CRSF_FRAMETYPE_PARAMETER_WRITE:
      if (crsf.ParameterUpdateData[1] == 0)
      {
        // special case for sending commit packet
        DBGVLN("send all lua params");
        sendELRSstatus();
      } else if (crsf.ParameterUpdateData[1] == 0x2E) {
        suppressCurrentLuaWarning();
      } else {
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
        populateHandler();
        sendLuaDevicePacket();
        break;

    case CRSF_FRAMETYPE_PARAMETER_READ: //param info
      {
        DBGVLN("Read lua param %u %u", crsf.ParameterUpdateData[1], crsf.ParameterUpdateData[2]);
        struct luaItem_command *field = (struct luaItem_command *)paramDefinitions[crsf.ParameterUpdateData[1]];
        if (field != 0 && (field->common.type & ~(CRSF_FIELD_HIDDEN|CRSF_FIELD_ELRS_HIDDEN)) == CRSF_COMMAND && crsf.ParameterUpdateData[2] == 0) {
          field->step = 0;
          field->info = "";
        }
        sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,crsf.ParameterUpdateData[2],(struct luaPropertiesCommon *)field);
      }
      break;

    default:
      DBGLN("Unknown LUA %x", crsf.ParameterUpdateData[0]);
  }

  UpdateParamReq = false;
  return true;
}

void sendLuaDevicePacket(void)
{
  uint8_t buffer[sizeof(txDeviceName) + sizeof(struct tagLuaDeviceProperties)];
  struct tagLuaDeviceProperties * const device = (struct tagLuaDeviceProperties * const)&buffer[sizeof(txDeviceName)];

  // Packet starts with device name
  memcpy(buffer, txDeviceName, sizeof(txDeviceName));
  // Followed by the device
  device->serialNo = htobe32(0x454C5253); // ['E', 'L', 'R', 'S'], seen [0x00, 0x0a, 0xe7, 0xc6] // "Serial 177-714694" (value is 714694)
  device->hardwareVer = 0; // unused currently by us, seen [ 0x00, 0x0b, 0x10, 0x01 ] // "Hardware: V 1.01" / "Bootloader: V 3.06"
  device->softwareVer = 0; // unused currently by ys, seen [ 0x00, 0x00, 0x05, 0x0f ] // "Firmware: V 5.15"
  device->fieldCnt = lastLuaField;

  crsf.packetQueueExtended(CRSF_FRAMETYPE_DEVICE_INFO, buffer, sizeof(buffer));
}

#endif
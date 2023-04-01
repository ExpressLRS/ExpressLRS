#include "lua.h"
#include "common.h"
#include "CRSF.h"
#include "logging.h"

#ifdef TARGET_RX
#include "telemetry.h"
#endif

extern CRSF crsf;

#ifdef TARGET_RX
extern Telemetry telemetry;
#endif

//LUA VARIABLES//

#ifdef TARGET_TX
static uint8_t luaWarningFlags = 0b00000000; //8 flag, 1 bit for each flag. set the bit to 1 to show specific warning. 3 MSB is for critical flag
static void (*devicePingCallback)() = nullptr;
#endif

#define LUA_MAX_PARAMS 32
static uint8_t parameterType;
static uint8_t parameterIndex;
static uint8_t parameterArg;
static volatile bool UpdateParamReq = false;

static struct luaPropertiesCommon *paramDefinitions[LUA_MAX_PARAMS] = {0}; // array of luaItem_*
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
  if(p1->dyn_name != NULL){
    return (uint8_t *)stpcpy((char *)next, p1->dyn_name) + 1;
  } else {
    return (uint8_t *)stpcpy((char *)next, p1->common.name) + 1;
  }
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
#ifdef TARGET_TX
  // Set the hidden flag
  chunkBuffer[3] |= luaData->type & CRSF_FIELD_HIDDEN ? 0x80 : 0;
  if (crsf.elrsLUAmode) {
    chunkBuffer[3] |= luaData->type & CRSF_FIELD_ELRS_HIDDEN ? 0x80 : 0;
  }
#else
  chunkBuffer[3] |= luaData->type;
  uint8_t paramInformation[DEVICE_INFORMATION_LENGTH];
#endif

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
      // re-fetch the lua data name, because luaFolderStructToArray will decide whether
      //to return the fixed name or dynamic name.
      chunkStart = luaFolderStructToArray(luaData, &chunkBuffer[4]);
      // subtract 1 because dataSize expects the end to not include the null
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
#ifdef TARGET_TX
  uint8_t chunkMax = CRSF::GetMaxPacketBytes() - 6 - 2;
#else
  uint8_t chunkMax = CRSF_MAX_PACKET_LEN - 6 - 2;
#endif
  // How many chunks needed to send this field (rounded up)
  uint8_t chunkCnt = (dataSize + chunkMax - 1) / chunkMax;
  // Data left to send is adjustedSize - chunks sent already
  uint8_t chunkSize = min((uint8_t)(dataSize - (fieldChunk * chunkMax)), chunkMax);

  // Move chunkStart back 2 bytes to add (FieldId + ChunksRemain) to each packet
  chunkStart = &chunkBuffer[fieldChunk * chunkMax];
  chunkStart[0] = luaData->id;                 // FieldId
  chunkStart[1] = chunkCnt - (fieldChunk + 1); // ChunksRemain
#ifdef TARGET_TX
  CRSF::packetQueueExtended(frameType, chunkStart, chunkSize + 2);
#else
  memcpy(paramInformation + sizeof(crsf_ext_header_t),chunkStart,chunkSize + 2);

  crsf.SetExtendedHeaderAndCrc(paramInformation, frameType, chunkSize + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + 2, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_CRSF_TRANSMITTER);

  telemetry.AppendTelemetryPackage(paramInformation);
#endif
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

void sendLuaCommandResponse(struct luaItem_command *cmd, luaCmdStep_e step, const char *message) {
  cmd->step = step;
  cmd->info = message;
  nextStatusChunk = 0;
  pushResponseChunk(cmd);
}

#ifdef TARGET_TX
static void luaSupressCriticalErrors()
{
  // clear the critical error bits of the warning flags
  luaWarningFlags &= 0b00011111;
}

void setLuaWarningFlag(lua_Flags flag, bool value)
{
  if (value)
  {
    luaWarningFlags |= 1 << (uint8_t)flag;
  }
  else
  {
    luaWarningFlags &= ~(1 << (uint8_t)flag);
  }
}

static void updateElrsFlags()
{
  setLuaWarningFlag(LUA_FLAG_MODEL_MATCH, connectionState == connected && connectionHasModelMatch == false);
  setLuaWarningFlag(LUA_FLAG_CONNECTED, connectionState == connected);
  setLuaWarningFlag(LUA_FLAG_ISARMED, crsf.IsArmed());
}

void sendELRSstatus()
{
  constexpr const char *messages[] = { //higher order = higher priority
    "",                   //status2 = connected status
    "",                   //status1, reserved for future use
    "Model Mismatch",     //warning3, model mismatch
    "[ ! Armed ! ]",      //warning2, AUX1 high / armed
    "",           //warning1, reserved for future use
    "Not while connected",  //critical warning3, trying to change a protected value while connected
    "Baud rate too low",  //critical warning2, changing packet rate and baud rate too low
    ""   //critical warning1, reserved for future use
  };
  const char * warningInfo = "";

  for (int i=7 ; i>=0 ; i--)
  {
      if (luaWarningFlags & (1<<i))
      {
          warningInfo = messages[i];
          break;
      }
  }
  uint8_t buffer[sizeof(tagLuaElrsParams) + strlen(warningInfo) + 1];
  struct tagLuaElrsParams * const params = (struct tagLuaElrsParams *)buffer;

  params->pktsBad = crsf.BadPktsCountResult;
  params->pktsGood = htobe16(crsf.GoodPktsCountResult);
  params->flags = luaWarningFlags;
  // to support sending a params.msg, buffer should be extended by the strlen of the message
  // and copied into params->msg (with trailing null)
  strcpy(params->msg, warningInfo);
  crsf.packetQueueExtended(0x2E, &buffer, sizeof(buffer));
}

void luaRegisterDevicePingCallback(void (*callback)())
{
  devicePingCallback = callback;
}

#endif

void ICACHE_RAM_ATTR luaParamUpdateReq(uint8_t type, uint8_t index, uint8_t arg)
{
  parameterType = type;
  parameterIndex = index;
  parameterArg = arg;
  UpdateParamReq = true;
}

void registerLUAParameter(void *definition, luaCallback callback, uint8_t parent)
{
  if (definition == nullptr)
  {
    static uint8_t agentLiteFolder[4+LUA_MAX_PARAMS+2] = "HooJ";
    static struct luaItem_folder luaAgentLite = {
        {(const char *)agentLiteFolder, CRSF_FOLDER},
    };

    paramDefinitions[0] = (struct luaPropertiesCommon *)&luaAgentLite;
    paramCallbacks[0] = 0;
    uint8_t *pos = agentLiteFolder + 4;
    for (int i=1;i<=lastLuaField;i++)
    {
      if (paramDefinitions[i]->parent == 0)
      {
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
  paramDefinitions[lastLuaField] = p;
  paramCallbacks[lastLuaField] = callback;
}

bool luaHandleUpdateParameter()
{
  if (UpdateParamReq == false)
  {
    return false;
  }

  switch(parameterType)
  {
    case CRSF_FRAMETYPE_PARAMETER_WRITE:
      if (parameterIndex == 0)
      {
        // special case for elrs linkstat request
#ifdef TARGET_TX
        DBGVLN("ELRS status request");
        updateElrsFlags();
        sendELRSstatus();
      } else if (parameterIndex == 0x2E) {
        luaSupressCriticalErrors();
#endif
      } else {
        uint8_t id = parameterIndex;
        uint8_t arg = parameterArg;
        struct luaPropertiesCommon *p = paramDefinitions[id];
        DBGLN("Set Lua [%s]=%u", p->name, arg);
        if (id < LUA_MAX_PARAMS && paramCallbacks[id]) {
          // While the command is executing, the handset will send `WRITE state=lcsQuery`.
          // paramCallbacks will set the value when nextStatusChunk == 0, or send any
          // remaining chunks when nextStatusChunk != 0
          if (arg == lcsQuery && nextStatusChunk != 0) {
            pushResponseChunk((struct luaItem_command *)p);
          } else {
            paramCallbacks[id](p, arg);
          }
        }
      }
      break;

    case CRSF_FRAMETYPE_DEVICE_PING:
#ifdef TARGET_TX
        devicePingCallback();
        luaSupressCriticalErrors();
#endif
        sendLuaDevicePacket();
        break;

    case CRSF_FRAMETYPE_PARAMETER_READ:
      {
        uint8_t fieldId = parameterIndex;
        uint8_t fieldChunk = parameterArg;
        DBGVLN("Read lua param %u %u", fieldId, fieldChunk);
        if (fieldId < LUA_MAX_PARAMS && paramDefinitions[fieldId])
        {
          struct luaItem_command *field = (struct luaItem_command *)paramDefinitions[fieldId];
          uint8_t dataType = field->common.type & CRSF_FIELD_TYPE_MASK;
          // On first chunk of a command, reset the step/info of the command
          if (dataType == CRSF_COMMAND && fieldChunk == 0)
          {
            field->step = lcsIdle;
            field->info = "";
          }
          sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, fieldChunk, &field->common);
        }
      }
      break;

    default:
      DBGLN("Unknown LUA %x", parameterType);
  }

  UpdateParamReq = false;
  return true;
}

void sendLuaDevicePacket(void)
{
  uint8_t deviceInformation[DEVICE_INFORMATION_LENGTH];
  crsf.GetDeviceInformation(deviceInformation, lastLuaField);
  // does append header + crc again so substract size from length
#ifdef TARGET_TX
  crsf.packetQueueExtended(CRSF_FRAMETYPE_DEVICE_INFO, deviceInformation + sizeof(crsf_ext_header_t), DEVICE_INFORMATION_PAYLOAD_LENGTH);
#else
  crsf.SetExtendedHeaderAndCrc(deviceInformation, CRSF_FRAMETYPE_DEVICE_INFO, DEVICE_INFORMATION_FRAME_SIZE, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_CRSF_TRANSMITTER);
  telemetry.AppendTelemetryPackage(deviceInformation);
#endif
}

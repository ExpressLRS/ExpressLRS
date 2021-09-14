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

static const void *paramDefinitions[32] = {0};
static luaCallback paramCallbacks[32] = {0};
static void (*populateHandler)() = 0;
static uint8_t lastLuaField = 0;
static uint8_t nextStatusChunk = 0;


static uint8_t getLuaTextSelectionStructToArray(const void * luaStruct, uint8_t *outarray){
  struct tagLuaItem_textSelection *p1 = (struct tagLuaItem_textSelection*)luaStruct;
  char *next = stpcpy((char *)outarray,p1->label1) + 1;
  next = stpcpy(next,p1->textOption) + 1;
  memcpy(next,&p1->luaProperties2,sizeof(p1->luaProperties2));
  next+=sizeof(p1->luaProperties2);
  *next++=0; // default value
  stpcpy(next,p1->label2);
  return p1->size;
}

static uint8_t getLuaCommandStructToArray(const void * luaStruct, uint8_t *outarray){
  struct tagLuaItem_command *p1 = (struct tagLuaItem_command*)luaStruct;
  char *next = stpcpy((char *)outarray,p1->label1) + 1;
  memcpy(next,&p1->luaProperties2,sizeof(p1->luaProperties2));
  next+=sizeof(p1->luaProperties2);
  stpcpy(next,p1->label2);
  return p1->size;
}

static uint8_t getLuaUint8StructToArray(const void * luaStruct, uint8_t *outarray){
  struct tagLuaItem_uint8 *p1 = (struct tagLuaItem_uint8*)luaStruct;
  char *next = stpcpy((char *)outarray,p1->label1) + 1;
  memcpy(next,&p1->luaProperties2,sizeof(p1->luaProperties2));
  next+=sizeof(p1->luaProperties2);
  *next++=0; // default value
  stpcpy(next,p1->label2);
  return p1->size;
}

static uint8_t getLuaUint16StructToArray(const void * luaStruct, uint8_t *outarray){
  struct tagLuaItem_uint16 *p1 = (struct tagLuaItem_uint16*)luaStruct;
  char *next = stpcpy((char *)outarray,p1->label1) + 1;
  memcpy(next,&p1->luaProperties2,sizeof(p1->luaProperties2));
  next+=sizeof(p1->luaProperties2);
  *next++=0; // default value
  stpcpy(next,p1->label2);
  return p1->size;
}

static uint8_t getLuaStringStructToArray(const void * luaStruct, uint8_t *outarray){
  struct tagLuaItem_string *p1 = (struct tagLuaItem_string*)luaStruct;
  char *next = stpcpy((char *)outarray,p1->label1) + 1;
  stpcpy(next,p1->label2);
  return p1->size;
}

static uint8_t getLuaFolderStructToArray(const void * luaStruct, uint8_t *outarray){
  struct tagLuaItem_folder *p1 = (struct tagLuaItem_folder*)luaStruct;
  stpcpy((char *)outarray,p1->label1);
  return p1->size;
}

static uint8_t sendCRSFparam(crsf_frame_type_e frameType, uint8_t fieldChunk, struct tagLuaProperties1 *luaData)
{
  uint8_t dataType = luaData->type & ~(CRSF_FIELD_HIDDEN|CRSF_FIELD_ELRS_HIDDEN);
  
  uint8_t chunkBuffer[256];

  chunkBuffer[0] = luaData->parent;
  chunkBuffer[1] = dataType;
  // Set the hidden flag
  chunkBuffer[1] |= luaData->type & CRSF_FIELD_HIDDEN ? 0x80 : 0;
  if (crsf.elrsLUAmode) {
    chunkBuffer[1] |= luaData->type & CRSF_FIELD_ELRS_HIDDEN ? 0x80 : 0;
  }

  uint8_t dataSize;
  switch(dataType) {
    case CRSF_TEXT_SELECTION:
      dataSize = getLuaTextSelectionStructToArray(luaData, chunkBuffer+2);
      break;
    case CRSF_COMMAND:
      dataSize = getLuaCommandStructToArray(luaData, chunkBuffer+2);
      break;
    case CRSF_UINT8:
      dataSize = getLuaUint8StructToArray(luaData,chunkBuffer+2);
      break;
    case CRSF_UINT16:
      dataSize = getLuaUint16StructToArray(luaData,chunkBuffer+2);
      break;
    case CRSF_STRING:
    case CRSF_INFO:
      dataSize = getLuaStringStructToArray(luaData,chunkBuffer+2);
      break;
    case CRSF_FOLDER:
      dataSize = getLuaFolderStructToArray(luaData,chunkBuffer+2);
      break;
    case CRSF_INT8:
    case CRSF_INT16:
    case CRSF_FLOAT:
    case CRSF_OUT_OF_RANGE:
    default:
      return 0;
  }

  // maximum number of chunked bytes that can be sent in one response
  // subtract the LUA overhead (8 bytes) bytes from the max packet size we can send
  uint16_t chunkMax = CRSF::GetMaxPacketBytes() - 8;

  // the adjusted size is 2 bytes less because the first 2 bytes of the LUA response are sent on every chunk
  uint8_t adjustedSize = dataSize - 2;

  // how many chunks to send this field
  uint8_t chunks = adjustedSize / chunkMax;
  uint8_t remainder = adjustedSize % chunkMax;
  if(remainder != 0) {
    chunks++;
  }

  // calculate this chunk size & packet size
  uint8_t chunkSize;    
  if (fieldChunk == chunks-1 && remainder != 0) {
    chunkSize = remainder;
  } else {
    chunkSize = chunkMax;
  }
  
  uint8_t packetSize = chunkSize + 2;
  uint8_t outBuffer[packetSize]; 

  outBuffer[0] = luaData->id;             // LUA data[3]
  outBuffer[1] = chunks - (fieldChunk+1); // remaining chunks to send;
  memcpy(outBuffer+2, chunkBuffer + (fieldChunk*chunkMax), chunkSize);

  CRSF::packetQueueExtended(frameType, outBuffer, packetSize);

  return chunks - (fieldChunk+1);
}

static void pushResponseChunk(struct tagLuaItem_command *cmd) {
  DBGVLN("sending response for id=%u chunk=%u status=%u", cmd->luaProperties1.id, nextStatusChunk, cmd->luaProperties2.status);
  if (sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,nextStatusChunk,(struct tagLuaProperties1 *)cmd) == 0) {
    nextStatusChunk = 0;
  } else {
    nextStatusChunk++;
  }
}

void sendLuaCommandResponse(struct tagLuaItem_command *cmd, uint8_t status, const char *message) {
  DBGVLN("Set Status=%u", status);
  cmd->luaProperties2.status = status;
  cmd->label2 = message;
  cmd->size = LUA_COMMAND_SIZE((*cmd));
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

uint8_t agentLiteFolder[4+32+2] = "HooJ";
struct tagLuaItem_folder luaAgentLite = {
    {0,0,(uint8_t)CRSF_FOLDER},//id,type
    (const char *)agentLiteFolder,
    0
};

void registerLUAParameter(void *definition, luaCallback callback, uint8_t parent)
{
  if (definition == NULL)
  {
    paramDefinitions[0] = &luaAgentLite;
    paramCallbacks[0] = 0;
    uint8_t *pos = agentLiteFolder + 4;
    for (int i=1;i<=lastLuaField;i++)
    {
      struct tagLuaProperties1 *p = (struct tagLuaProperties1 *)paramDefinitions[i];
      if (p->parent == 0) {
        *pos++ = i;
      }
    }
    *pos++ = 0xFF;
    *pos++ = 0;
    luaAgentLite.size = 4 + strlen(luaAgentLite.label1) + 1;
    return;
  }
  struct tagLuaProperties1 *p = (struct tagLuaProperties1 *)definition;
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
        DBGVLN("Write lua param %u %u", crsf.ParameterUpdateData[1], crsf.ParameterUpdateData[2]);
        uint8_t param = crsf.ParameterUpdateData[1];
        if (param < 32 && paramCallbacks[param] != 0) {
          if (crsf.ParameterUpdateData[2] == 6 && nextStatusChunk != 0) {
            pushResponseChunk((struct tagLuaItem_command *)paramDefinitions[param]);
          } else {
            paramCallbacks[param](param, crsf.ParameterUpdateData[2]);
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
        struct tagLuaItem_command *field = (struct tagLuaItem_command *)paramDefinitions[crsf.ParameterUpdateData[1]];
        if (field != 0 && (field->luaProperties1.type & ~(CRSF_FIELD_HIDDEN|CRSF_FIELD_ELRS_HIDDEN)) == CRSF_COMMAND && crsf.ParameterUpdateData[2] == 0) {
          field->luaProperties2.status = 0;
          field->label2 = "";
          field->size = LUA_COMMAND_SIZE((*field));
        }
        sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,crsf.ParameterUpdateData[2],(struct tagLuaProperties1 *)field);
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

void setLuaTextSelectionValue(struct tagLuaItem_textSelection *luaStruct, uint8_t newvalue){
    luaStruct->luaProperties2.value = newvalue;
}
void setLuaCommandValue(struct tagLuaItem_command *luaStruct, uint8_t newvalue){
    luaStruct->luaProperties2.status = newvalue;
}
void setLuaUint8Value(struct tagLuaItem_uint8 *luaStruct, uint8_t newvalue){
    luaStruct->luaProperties2.value = newvalue;
}
void setLuaUint16Value(struct tagLuaItem_uint16 *luaStruct, uint16_t newvalue){
    luaStruct->luaProperties2.value = (newvalue >> 8) | (newvalue << 8);
}
void setLuaStringValue(struct tagLuaItem_string *luaStruct,const char *newvalue){
    luaStruct->label2 = newvalue;
}
void setLuaCommandInfo(struct tagLuaItem_command *luaStruct,const char *newvalue){
    luaStruct->label2 = newvalue;
}
#endif
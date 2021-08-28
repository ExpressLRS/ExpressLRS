#ifdef TARGET_TX

#include "lua.h"
#include "CRSF.h"
#include "logging.h"

const char txDeviceName[] = TX_DEVICE_NAME;

extern CRSF crsf;

static uint8_t allLUAparamSent = 0;
static volatile bool UpdateParamReq = false;

//LUA VARIABLES//
#define LUA_PKTCOUNT_INTERVAL_MS 1000LU
static uint8_t luaWarningFLags = false;
static uint8_t suppressedLuaWarningFlags = true;

static const void *paramDefinitions[32] = {0};
static luaCallback paramCallbacks[32] = {0};
static void (*populateHandler)() = 0;
static uint8_t lastLuaField = 0;

#define TYPE(T) (struct T *)p,((struct T *)p)->size
static uint8_t iterateLUAparams(uint8_t idx, uint8_t chunk)
{
  uint8_t retval = 0;
  struct tagLuaProperties1 *p = (struct tagLuaProperties1 *)paramDefinitions[idx];
  if (p != 0) {
    switch(p->type) {
      case CRSF_UINT8:
        retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_UINT8,TYPE(tagLuaItem_uint8));
        break;
      case CRSF_UINT16:
        retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_UINT16,TYPE(tagLuaItem_uint16));
        break;
      case CRSF_STRING:
        retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_STRING,TYPE(tagLuaItem_string));
        break;
      case CRSF_INFO:
        retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_INFO,TYPE(tagLuaItem_string));
        break;
      case CRSF_COMMAND:
        retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_COMMAND,TYPE(tagLuaItem_command));
        break;
      case CRSF_TEXT_SELECTION:
        retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_TEXT_SELECTION,TYPE(tagLuaItem_textSelection));
        break;
      case CRSF_FOLDER:
        retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_FOLDER,TYPE(tagLuaItem_folder));
        break;
    }
    if(retval == 0 && idx == lastLuaField){
      allLUAparamSent = 1;
    }
  }
  return retval;
}
#undef TYPE

void sendLuaFieldCrsf(uint8_t idx, uint8_t chunk){
  if(!allLUAparamSent){
    iterateLUAparams(idx,chunk);
  }
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
      allLUAparamSent = 0;
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
      allLUAparamSent = 0;
      if (crsf.ParameterUpdateData[1] == 0)
      {
        // special case for sending commit packet
        DBGVLN("send all lua params");
        sendELRSstatus();
      } else if (crsf.ParameterUpdateData[1] == 0x2E) {
        suppressCurrentLuaWarning();
      } else {
        uint8_t param = crsf.ParameterUpdateData[1];
        if (param < 32 && paramCallbacks[param] != 0) {
          paramCallbacks[param](param, crsf.ParameterUpdateData[2]);
        }
      }
      break;

    case CRSF_FRAMETYPE_DEVICE_PING:
        allLUAparamSent = 0;
        populateHandler();
        sendLuaDevicePacket();
        break;

    case CRSF_FRAMETYPE_PARAMETER_READ: //param info
      sendLuaFieldCrsf(crsf.ParameterUpdateData[1], crsf.ParameterUpdateData[2]);
      break;
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
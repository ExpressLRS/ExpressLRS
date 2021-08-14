#ifdef TARGET_TX

#include "lua.h"

#include "CRSF.h"

extern CRSF crsf;

static uint8_t allLUAparamSent = 0;  
static volatile bool UpdateParamReq = false;

//LUA VARIABLES//
#define LUA_PKTCOUNT_INTERVAL_MS 1000LU
static bool luaWarningFLags = false;
static bool suppressedLuaWarningFlags = true;

static const void *paramDefinitions[32] = {0};
static luaCallback paramCallbacks[32] = {0};
static void (*populateHandler)() = 0;
static uint8_t lastLuaField = 0;

static struct tagLuaDevice luaDevice = {
    "ELRS",
    {{0},0},
    LUA_DEVICE_SIZE(luaDevice)
};

#define EDITABLE(T) ((struct T *)p)->luaProperties1.id,((struct T *)p)->editableFlag
static void setLUAEditFlag(const struct tagLuaProperties1 *p)
{
  switch(p->type) {
    case CRSF_UINT8:
      crsf.setEditableFlag(EDITABLE(tagLuaItem_uint8));
      break;
    case CRSF_UINT16:
      crsf.setEditableFlag(EDITABLE(tagLuaItem_uint16));
      break;
    case CRSF_STRING:
    case CRSF_INFO:
      crsf.setEditableFlag(EDITABLE(tagLuaItem_string));
      break;
    case CRSF_COMMAND:
      crsf.setEditableFlag(EDITABLE(tagLuaItem_command));
      break;
    case CRSF_TEXT_SELECTION:
      crsf.setEditableFlag(EDITABLE(tagLuaItem_textSelection));
      break;
  }
}
#undef EDITABLE

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
  uint8_t luaParams[] = {(uint8_t)crsf.BadPktsCountResult,
                         (uint8_t)((crsf.GoodPktsCountResult & 0xFF00) >> 8),
                         (uint8_t)(crsf.GoodPktsCountResult & 0xFF),
                         (uint8_t)(getLuaWarning())};

  crsf.sendELRSparam(luaParams,4, 0x2E, getLuaWarning() ? "beta" : " ", 4); //*elrsinfo is the info that we want to pass when there is getluawarning()
}

void ICACHE_RAM_ATTR luaParamUpdateReq()
{
  UpdateParamReq = true;
}

void registerLUAParameter(const void *definition, luaCallback callback)
{
  const struct tagLuaProperties1 *p = (const struct tagLuaProperties1 *)definition;
  paramDefinitions[p->id] = definition;
  paramCallbacks[p->id] = callback;
  lastLuaField = max(lastLuaField, p->id);
  setLUAEditFlag(p);
  luaDevice.luaDeviceProperties.fieldamount = lastLuaField;
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
  #ifndef DEBUG_SUPPRESS
        Serial.println("send all lua params");
  #endif
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
        crsf.sendCRSFdevice(&luaDevice,luaDevice.size);
        break;

    case CRSF_FRAMETYPE_PARAMETER_READ: //param info
      sendLuaFieldCrsf(crsf.ParameterUpdateData[1], crsf.ParameterUpdateData[2]);
      break;
  }

  UpdateParamReq = false;
  return true;
}

#endif
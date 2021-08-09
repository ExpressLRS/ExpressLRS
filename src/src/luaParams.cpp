
#include "luaParams.h"

#include "CRSF.h"

extern CRSF crsf;

volatile uint8_t allLUAparamSent = 0;  

//LUA VARIABLES//
static bool luaWarningFLags = false;
static bool suppressedLuaWarningFlags = true;

const char thisCommit[] = {LATEST_COMMIT, 0};
const struct tagLuaDevice luaDevice = {
    "ELRS",
    {{0},LUA_FIELD_AMOUNT},
    LUA_DEVICE_SIZE(luaDevice)
};


const struct tagLuaItem_textSelection luaAirRate = {
    {1,0,0,(uint8_t)CRSF_TEXT_SELECTION}, //id,chunk,parent,type
    "P.Rate",
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) 
    "200(-112dbm);100(-117dbm);50(-120dbm);25(-123dbm)",
#elif defined(Regulatory_Domain_ISM_2400)
    "500(-105dbm);250(-108dbm);150(-112dbm);50(-117dbm)",
#endif
    {0,0,3,0},//value,min,max,default
    "Hz",
    LUA_TEXTSELECTION_SIZE(luaAirRate),
    1
};
const struct tagLuaItem_textSelection luaTlmRate = {
    {2,0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,chunk,parent,type
    "T.Rate",
    "off;1/128;1/64;1/32;1/16;1/8;1/4;1/2",
    {0,0,7,0},//value,min,max,default
    " ",
    LUA_TEXTSELECTION_SIZE(luaTlmRate),
    1
};
const struct tagLuaItem_textSelection luaPower = {
    {3,0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,chunk,parent,type
    "Pwr",
    "10;25;50;100;250;500;1000;2000",
    {0,0,7,0},//value,min,max,default
    "mW",
    LUA_TEXTSELECTION_SIZE(luaPower),
    1
};

const struct tagLuaItem_command luaBind = {
    {4,0,0,(uint8_t)CRSF_COMMAND},//id,chunk,parent,type
    "Bind",
    {0,200},//status,timeout
    " ",
    LUA_COMMAND_SIZE(luaBind),
    1
};
const struct tagLuaItem_command luaWebUpdate = {
    {5,0,0,(uint8_t)CRSF_COMMAND},//id,chunk,parent,type
    "Update",
    {0,200},//status,timeout
    " ",
    LUA_COMMAND_SIZE(luaWebUpdate),
    1
};

const struct tagLuaItem_uint8 luaBadPkt = {
    {6,0,0,(uint8_t)CRSF_UINT8},//id,chunk,parent,type
    "Bad",
    {0,0,254,0},//value,min,max,default
    " ",
    LUA_UINT8_SIZE(luaBadPkt),
    0
};
const struct tagLuaItem_uint16 luaGoodPkt = {
    {7,0,0,(uint8_t)CRSF_UINT16},//id,chunk,parent,type
    "Good",
    {0,0,511,0},//value,min,max,default
    " ",
    LUA_UINT16_SIZE(luaGoodPkt),
    0
};

const struct tagLuaItem_string luaCommit = {
    {8,0,0,(uint8_t)CRSF_STRING},//id,chunk,parent,type
    thisCommit,
    " ",
    LUA_STRING_SIZE(luaCommit),
    0
};

const void *luaParams[] = {
    &luaAirRate,
    &luaTlmRate,
    &luaPower,
    &luaBind,
    &luaWebUpdate,
    &luaBadPkt,
    &luaGoodPkt,
    &luaCommit
};



#define EDITABLE(T) ((struct T *)p)->luaProperties1.id,((struct T *)p)->editableFlag
void setLUAEditFlags(uint8_t param)
{
  struct tagLuaProperties1 *p = (struct tagLuaProperties1 *)luaParams[param-1];
  switch(p->type) {
    case CRSF_UINT8:
      crsf.setEditableFlag(EDITABLE(tagLuaItem_uint8));
      break;
    case CRSF_UINT16:
      crsf.setEditableFlag(EDITABLE(tagLuaItem_uint16));
      break;
    case CRSF_STRING:
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
  struct tagLuaProperties1 *p = (struct tagLuaProperties1 *)luaParams[idx-1];
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
    case CRSF_COMMAND:
      retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_COMMAND,TYPE(tagLuaItem_command));
      break;
    case CRSF_TEXT_SELECTION:
      retval = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_TEXT_SELECTION,TYPE(tagLuaItem_textSelection));
      break;
  }

  if(retval == 0 && idx == LUA_FIELD_AMOUNT){
    allLUAparamSent = 1;
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

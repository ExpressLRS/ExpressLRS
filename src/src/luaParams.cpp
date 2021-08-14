#ifdef TARGET_TX

#include "lua.h"

#include "CRSF.h"

const char thisCommit[] = {LATEST_COMMIT, 0};

struct tagLuaItem_textSelection luaAirRate = {
    {1,(uint8_t)CRSF_TEXT_SELECTION}, //id,chunk,parent,type
    "P.Rate",
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) 
    "200(-112dbm);100(-117dbm);50(-120dbm);25(-123dbm)",
#elif defined(Regulatory_Domain_ISM_2400)
    "500(-105dbm);250(-108dbm);150(-112dbm);50(-117dbm)",
#endif
    {0,0,3},//value,min,max,default
    "Hz",
    LUA_TEXTSELECTION_SIZE(luaAirRate)
};
struct tagLuaItem_textSelection luaTlmRate = {
    {2,(uint8_t)CRSF_TEXT_SELECTION},//id,chunk,parent,type
    "T.Rate",
    "off;1/128;1/64;1/32;1/16;1/8;1/4;1/2",
    {0,0,7},//value,min,max,default
    " ",
    LUA_TEXTSELECTION_SIZE(luaTlmRate)
};
struct tagLuaItem_textSelection luaPower = {
    {3,(uint8_t)CRSF_TEXT_SELECTION},//id,chunk,parent,type
    "Pwr",
    "10;25;50;100;250;500;1000;2000",
    {0,0,7},//value,min,max,default
    "mW",
    LUA_TEXTSELECTION_SIZE(luaPower)
};

struct tagLuaItem_command luaBind = {
    {4,(uint8_t)CRSF_COMMAND},//id,chunk,parent,type
    "Bind",
    {0,200},//status,timeout
    " ",
    LUA_COMMAND_SIZE(luaBind)
};
struct tagLuaItem_command luaWebUpdate = {
    {5,(uint8_t)CRSF_COMMAND},//id,chunk,parent,type
    "Update",
    {0,200},//status,timeout
    " ",
    LUA_COMMAND_SIZE(luaWebUpdate)
};

struct tagLuaItem_string luaInfo = {
    {6,(uint8_t)CRSF_INFO},//id,chunk,parent,type
    thisCommit,
    thisCommit,
    LUA_STRING_SIZE(luaInfo)
};

#endif

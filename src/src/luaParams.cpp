#ifdef TARGET_TX

#include "lua.h"

#include "CRSF.h"

const char thisCommit[] = {LATEST_COMMIT, 0};

extern "C" const struct tagLuaItem_textSelection luaAirRate = {
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
extern "C" const struct tagLuaItem_textSelection luaTlmRate = {
    {2,0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,chunk,parent,type
    "T.Rate",
    "off;1/128;1/64;1/32;1/16;1/8;1/4;1/2",
    {0,0,7,0},//value,min,max,default
    " ",
    LUA_TEXTSELECTION_SIZE(luaTlmRate),
    1
};
extern "C" const struct tagLuaItem_textSelection luaPower = {
    {3,0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,chunk,parent,type
    "Pwr",
    "10;25;50;100;250;500;1000;2000",
    {0,0,7,0},//value,min,max,default
    "mW",
    LUA_TEXTSELECTION_SIZE(luaPower),
    1
};

extern "C" const struct tagLuaItem_command luaBind = {
    {4,0,0,(uint8_t)CRSF_COMMAND},//id,chunk,parent,type
    "Bind",
    {0,200},//status,timeout
    " ",
    LUA_COMMAND_SIZE(luaBind),
    1
};
extern "C" const struct tagLuaItem_command luaWebUpdate = {
    {5,0,0,(uint8_t)CRSF_COMMAND},//id,chunk,parent,type
    "Update",
    {0,200},//status,timeout
    " ",
    LUA_COMMAND_SIZE(luaWebUpdate),
    1
};

extern "C" const struct tagLuaItem_uint8 luaBadPkt = {
    {6,0,0,(uint8_t)CRSF_UINT8},//id,chunk,parent,type
    "Bad",
    {0,0,254,0},//value,min,max,default
    " ",
    LUA_UINT8_SIZE(luaBadPkt),
    0
};
extern "C" const struct tagLuaItem_uint16 luaGoodPkt = {
    {7,0,0,(uint8_t)CRSF_UINT16},//id,chunk,parent,type
    "Good",
    {0,0,511,0},//value,min,max,default
    " ",
    LUA_UINT16_SIZE(luaGoodPkt),
    0
};

extern "C" struct tagLuaItem_string luaCommit = {
    {8,0,0,(uint8_t)CRSF_INFO},//id,chunk,parent,type
    thisCommit,
    thisCommit,
    LUA_STRING_SIZE(luaCommit),
    0
};

#endif

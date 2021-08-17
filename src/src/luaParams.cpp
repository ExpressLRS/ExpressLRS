#ifdef TARGET_TX

#include "lua.h"

#include "CRSF.h"

const char thisCommit[] = {LATEST_COMMIT, 0};
const char thisVersion[] = {LATEST_VERSION, 0};
const char emptySpace[1] = {0};
#ifdef PLATFORM_STM32
typedef enum
{
    LUA_AIR_RATE = 1,
    LUA_TLM_RATE,
    LUA_POWER,
    LUA_BIND,
    LUA_WEB_UPDATE,
    LUA_INFO,
    LUA_ELRSversion    
} lua_param_id_e;
#else
typedef enum
{
    LUA_AIR_RATE = 1,
    LUA_TLM_RATE,
    LUA_POWER,
    LUA_BIND,
    LUA_INFO,
    LUA_ELRSversion    
} lua_param_id_e;
#endif

struct tagLuaItem_textSelection luaAirRate = {
    {(uint8_t)LUA_AIR_RATE,(uint8_t)CRSF_TEXT_SELECTION}, //id,type
    "P.Rate",
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) 
    "200(-112dbm);100(-117dbm);50(-120dbm);25(-123dbm)",
#elif defined(Regulatory_Domain_ISM_2400)
    "500(-105dbm);250(-108dbm);150(-112dbm);50(-117dbm)",
#endif
    {0,0,3},//value,min,max
    "Hz",
    LUA_TEXTSELECTION_SIZE(luaAirRate)
};
struct tagLuaItem_textSelection luaTlmRate = {
    {(uint8_t)LUA_TLM_RATE,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "T.Rate",
    "off;1/128;1/64;1/32;1/16;1/8;1/4;1/2",
    {0,0,7},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaTlmRate)
};
struct tagLuaItem_textSelection luaPower = {
    {(uint8_t)LUA_POWER,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Pwr",
    "10;25;50;100;250;500;1000;2000",
    {0,0,7},//value,min,max
    "mW",
    LUA_TEXTSELECTION_SIZE(luaPower)
};

struct tagLuaItem_command luaBind = {
    {(uint8_t)LUA_BIND,(uint8_t)CRSF_COMMAND},//id,type
    "Bind",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaBind)
};

struct tagLuaItem_string luaInfo = {
    {(uint8_t)LUA_INFO,(uint8_t)CRSF_INFO},//id,type
    thisCommit,
    thisCommit,
    LUA_STRING_SIZE(luaInfo)
};
struct tagLuaItem_string luaELRSversion = {
    {(uint8_t)LUA_ELRSversion,(uint8_t)CRSF_INFO},//id,type
    thisVersion,
    emptySpace,
    LUA_STRING_SIZE(luaELRSversion)
};


#ifdef PLATFORM_STM32
struct tagLuaItem_command luaWebUpdate = {
    {(uint8_t)LUA_WEB_UPDATE,(uint8_t)CRSF_COMMAND},//id,type
    "Update",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaWebUpdate)
};
#endif
#endif

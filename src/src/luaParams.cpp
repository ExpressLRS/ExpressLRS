#ifdef TARGET_TX

#include "lua.h"

#include "CRSF.h"

const char thisCommit[] = {LATEST_COMMIT, 0};
const char thisVersion[] = {LATEST_VERSION, 0};
const char emptySpace[1] = {0};

struct tagLuaItem_textSelection luaAirRate = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION}, //id,type
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
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "T.Rate",
    "Off;1/128;1/64;1/32;1/16;1/8;1/4;1/2",
    {0,0,7},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaTlmRate)
};
//----------------------------POWER------------------
struct tagLuaItem_folder luaPowerFolder = {
    {0,0,(uint8_t)CRSF_FOLDER},//id,type
    "TX Power",
    LUA_FOLDER_SIZE(luaPowerFolder)
};
struct tagLuaItem_textSelection luaPower = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Max Power",
    "10;25;50;100;250;500;1000;2000",
    {0,0,7},//value,min,max
    "mW",
    LUA_TEXTSELECTION_SIZE(luaPower)
};
struct tagLuaItem_textSelection luaDynamicPower = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Dynamic",
    "Off;On;AUX9;AUX10;AUX11;AUX12",
    {0,0,5},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaDynamicPower)
};
//----------------------------POWER------------------

struct tagLuaItem_textSelection luaSwitch = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Switch Mode",
    "Hybrid;Wide",
    {0,0,1},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaSwitch)
};
struct tagLuaItem_textSelection luaModelMatch = {
    {5,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Model Match",
    "Off;On",
    {0,0,1},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaModelMatch)
};
struct tagLuaItem_command luaBind = {
    {0,0,(uint8_t)CRSF_COMMAND},//id,type
    "Bind",
    {0,200},//status,timeout
    "binding...",
    LUA_COMMAND_SIZE(luaBind)
};
struct tagLuaItem_string luaInfo = {
    {0,0,(uint8_t)CRSF_INFO},//id,type
    "Bad/Good",
    thisCommit,
    LUA_STRING_SIZE(luaInfo)
};
struct tagLuaItem_string luaELRSversion = {
    {0,0,(uint8_t)CRSF_INFO},//id,type
    thisVersion,
    thisCommit,
    LUA_STRING_SIZE(luaELRSversion)
};

#ifdef PLATFORM_ESP32
struct tagLuaItem_command luaWebUpdate = {
    {0,0,(uint8_t)CRSF_COMMAND},//id,type
    "WiFi Update",
    {0,200},//status,timeout
    "Proceed WiFi Update Mode?",
    LUA_COMMAND_SIZE(luaWebUpdate)
};

struct tagLuaItem_command luaBLEJoystick = {
    {0,0,(uint8_t)CRSF_COMMAND},//id,type
    "BLE Joystick",
    {0,200},//status,timeout
    "Enable BLE Joystick?",
    LUA_COMMAND_SIZE(luaBLEJoystick)
};
#endif

//----------------------------VTX ADMINISTRATOR------------------
struct tagLuaItem_folder luaVtxFolder = {
    {0,0,(uint8_t)CRSF_FOLDER},//id,type
    "VTX ADMINISTRATOR",
    LUA_FOLDER_SIZE(luaVtxFolder)
};

struct tagLuaItem_textSelection luaVtxBand = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Band",
    "Off;A;B;E;F;R;L",
    {0,0,6},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaVtxBand)
};

struct tagLuaItem_textSelection luaVtxChannel = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Channel",
    "1;2;3;4;5;6;7;8",
    {0,0,7},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaVtxChannel)
};

struct tagLuaItem_textSelection luaVtxPwr = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Pwr Lvl",
    "-;1;2;3;4;5;6;7;8",
    {0,0,8},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaVtxPwr)
};

struct tagLuaItem_textSelection luaVtxPit = {
    {0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,type
    "Pitmode",
    "Off;On",
    {0,0,1},//value,min,max
    emptySpace,
    LUA_TEXTSELECTION_SIZE(luaVtxPit)
};

struct tagLuaItem_command luaVtxSend = {
    {0,0,(uint8_t)CRSF_COMMAND},//id,type
    "Send VTx",
    {0,200},//status,timeout
    emptySpace,
    LUA_COMMAND_SIZE(luaVtxSend)
};

//----------------------------VTX ADMINISTRATOR------------------

#endif

#pragma once

#include "crsf_protocol.h"

extern struct tagLuaItem_textSelection luaAirRate;
extern struct tagLuaItem_textSelection luaTlmRate;
// Commented out for now until we add more switch options
// extern struct tagLuaItem_textSelection luaSwitch;
extern struct tagLuaItem_textSelection luaModelMatch;
extern struct tagLuaItem_command luaBind;
extern struct tagLuaItem_string luaInfo;
extern struct tagLuaItem_string luaELRSversion;

#ifdef PLATFORM_ESP32
extern struct tagLuaItem_command luaWebUpdate;
extern struct tagLuaItem_command luaBLEJoystick;
#endif

//----------------------------POWER------------------
extern struct tagLuaItem_folder luaPowerFolder;
extern struct tagLuaItem_textSelection luaPower;
extern struct tagLuaItem_textSelection luaDynamicPower;
//----------------------------POWER------------------

//----------------------------VTX ADMINISTRATOR------------------
extern struct tagLuaItem_folder luaVtxFolder;
extern struct tagLuaItem_textSelection luaVtxBand;
extern struct tagLuaItem_textSelection luaVtxChannel;
extern struct tagLuaItem_textSelection luaVtxPwr;
extern struct tagLuaItem_textSelection luaVtxPit;
extern struct tagLuaItem_command luaVtxSend;
//----------------------------VTX ADMINISTRATOR------------------

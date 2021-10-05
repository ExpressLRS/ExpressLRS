#pragma once

#include "crsf_protocol.h"

extern struct tagLuaItem_textSelection luaAirRate;
extern struct tagLuaItem_textSelection luaTlmRate;
extern struct tagLuaItem_textSelection luaSwitch;
extern struct tagLuaItem_textSelection luaModelMatch;
extern struct tagLuaItem_command luaBind;
extern struct tagLuaItem_string luaInfo;
extern struct tagLuaItem_string luaELRSversion;

#if defined(PLATFORM_ESP32)
extern struct tagLuaItem_command luaBLEJoystick;
#endif

//---------------------------- WiFi -----------------------------
extern struct tagLuaItem_folder luaWiFiFolder;
#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
extern struct tagLuaItem_command luaWebUpdate;
#endif
extern struct tagLuaItem_command luaTxBackpackUpdate;
extern struct tagLuaItem_command luaVRxBackpackUpdate;
//---------------------------- WiFi -----------------------------

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

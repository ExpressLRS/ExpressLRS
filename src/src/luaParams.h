#pragma once

#include "crsf_protocol.h"

extern struct tagLuaItem_textSelection luaAirRate;
extern struct tagLuaItem_textSelection luaTlmRate;
extern struct tagLuaItem_textSelection luaPower;
// Commented out for now until we add more switch options
// extern struct tagLuaItem_textSelection luaSwitch;
extern struct tagLuaItem_textSelection luaModelMatch;
extern struct tagLuaItem_uint8 luaSetRXModel;
extern struct tagLuaItem_command luaBind;
extern struct tagLuaItem_string luaInfo;
extern struct tagLuaItem_string luaELRSversion;

#ifdef PLATFORM_ESP32
extern struct tagLuaItem_command luaWebUpdate;
#endif
#pragma once
#ifndef H_LUAPARAMS
#define H_LUAPARAMS
#include "common.h"
#include "crsf_protocol.h"
#define LUA_FIELD_AMOUNT 6

void setLuaTextSelectionValue(struct tagLuaItem_textSelection *textSelectionStruct, uint8_t newvalue);
void setLuaCommandValue(struct tagLuaItem_command *textSelectionStruct, uint8_t newvalue);
extern struct tagLuaDevice luaDevice;
extern struct tagLuaItem_textSelection luaAirRate;
extern struct tagLuaItem_textSelection luaTlmRate;
extern struct tagLuaItem_textSelection luaPower;
extern struct tagLuaItem_textSelection luaReg;
extern struct tagLuaItem_command luaBind;
extern struct tagLuaItem_command luaWebUpdate;


#endif
#pragma once
#ifndef H_LUAPARAMS
#define H_LUAPARAMS
#include "common.h"
#include "crsf_protocol.h"
#define LUA_DEVICE_SIZE(X) sizeof(tagLuaDeviceProperties)+strlen(X.label1)+1
#define LUA_TEXTSELECTION_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+strlen(X.textOption)+1+sizeof(tagLuaTextSelectionProperties)+strlen(X.label2)+1
#define LUA_COMMAND_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaCommandProperties)+strlen(X.label2)+1
#define LUA_UINT8_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaUint8Properties)+strlen(X.label2)+1
#define LUA_UINT16_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaUint16Properties)+strlen(X.label2)+1
#define LUA_STRING_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+strlen(X.label2)+1

/**we dont use this yet for OUR LUA
#define LUA_INT8_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaInt8Properties)+strlen(X.label2)+1
#define LUA_UINT16_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaUint16Properties)+strlen(X.label2)+1
#define LUA_INT16_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaInt16Properties)+strlen(X.label2)+1
#define LUA_FLOAT_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaFloatProperties)+strlen(X.label2)+1
*/


#define LUA_FIELD_AMOUNT 8


void setLuaTextSelectionValue(struct tagLuaItem_textSelection *textSelectionStruct, uint8_t newvalue);
void setLuaCommandValue(struct tagLuaItem_command *textSelectionStruct, uint8_t newvalue);
void setLuaUint8Value(struct tagLuaItem_uint8 *luaStruct, uint8_t newvalue);
void setLuaUint16Value(struct tagLuaItem_uint16 *luaStruct, uint16_t newvalue);

extern struct tagLuaDevice luaDevice;
extern struct tagLuaItem_textSelection luaAirRate;
extern struct tagLuaItem_textSelection luaTlmRate;
extern struct tagLuaItem_textSelection luaPower;
extern struct tagLuaItem_textSelection luaReg;
extern struct tagLuaItem_command luaBind;
extern struct tagLuaItem_command luaWebUpdate;
extern struct tagLuaItem_uint8 luaBadPkt;
extern struct tagLuaItem_uint16 luaGoodPkt;
extern struct tagLuaItem_string luaCommit;


#endif
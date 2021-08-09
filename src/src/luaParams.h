#pragma once
#ifndef H_LUAPARAMS
#define H_LUAPARAMS
#include "common.h"
#include "crsf_protocol.h"
#define LUA_DEVICE_SIZE(X) (uint8_t)(sizeof(tagLuaDeviceProperties)+strlen(X.label1)+1)
#define LUA_TEXTSELECTION_SIZE(X) (uint8_t)(sizeof(tagLuaProperties1)+strlen(X.label1)+1+strlen(X.textOption)+1+sizeof(tagLuaTextSelectionProperties)+strlen(X.label2)+1)
#define LUA_COMMAND_SIZE(X) (uint8_t)(sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaCommandProperties)+strlen(X.label2)+1)
#define LUA_UINT8_SIZE(X) (uint8_t)(sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaUint8Properties)+strlen(X.label2)+1)
#define LUA_UINT16_SIZE(X) (uint8_t)(sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaUint16Properties)+strlen(X.label2)+1)
#define LUA_STRING_SIZE(X) (uint8_t)(sizeof(tagLuaProperties1)+strlen(X.label1)+1+strlen(X.label2)+1)

/**we dont use this yet for OUR LUA
#define LUA_INT8_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaInt8Properties)+strlen(X.label2)+1
#define LUA_UINT16_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaUint16Properties)+strlen(X.label2)+1
#define LUA_INT16_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaInt16Properties)+strlen(X.label2)+1
#define LUA_FLOAT_SIZE(X) sizeof(tagLuaProperties1)+strlen(X.label1)+1+sizeof(tagLuaFloatProperties)+strlen(X.label2)+1
*/


#define LUA_FIELD_AMOUNT 8

extern const struct tagLuaDevice luaDevice;
extern const struct tagLuaItem_textSelection luaAirRate;
extern const struct tagLuaItem_textSelection luaTlmRate;
extern const struct tagLuaItem_textSelection luaPower;
extern const struct tagLuaItem_command luaBind;
extern const struct tagLuaItem_command luaWebUpdate;
extern const struct tagLuaItem_uint8 luaBadPkt;
extern const struct tagLuaItem_uint16 luaGoodPkt;
extern const struct tagLuaItem_string luaCommit;

extern const void *luaParams[LUA_FIELD_AMOUNT];

extern volatile uint8_t allLUAparamSent;
extern void setLUAEditFlags(uint8_t param);
extern void sendLuaFieldCrsf(uint8_t idx, uint8_t chunk);

extern void suppressCurrentLuaWarning(void);
extern bool getLuaWarning(void);

#endif
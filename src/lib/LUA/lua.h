#pragma once

#ifdef TARGET_TX

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

extern void sendLuaFieldCrsf(uint8_t idx, uint8_t chunk);

extern void suppressCurrentLuaWarning(void);
extern bool getLuaWarning(void);
extern void ICACHE_RAM_ATTR luaParamUpdateReq();
extern void luaHandleUpdateParameter();

void registerLUAPopulateParams(void (*populate)());

typedef void (*luaCallback)(uint8_t id, uint8_t arg);
void registerLUAParameter(const void *definition, luaCallback callback);

#endif

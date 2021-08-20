#pragma once

#ifdef TARGET_TX

#include "targets.h"
#include "crsf_protocol.h"

#define LUA_DEVICE_SIZE(X) (uint8_t)(sizeof(tagLuaDeviceProperties)+strlen(X.label1)+1)
#define LUA_TEXTSELECTION_SIZE(X) (uint8_t)(4+strlen(X.label1)+1+strlen(X.textOption)+1+sizeof(tagLuaTextSelectionProperties)+1+strlen(X.label2)+1)
#define LUA_COMMAND_SIZE(X) (uint8_t)(4+strlen(X.label1)+1+sizeof(tagLuaCommandProperties)+strlen(X.label2)+1)
#define LUA_UINT8_SIZE(X) (uint8_t)(4+strlen(X.label1)+1+sizeof(tagLuaUint8Properties)+1+strlen(X.label2)+1)
#define LUA_UINT16_SIZE(X) (uint8_t)(4+strlen(X.label1)+1+sizeof(tagLuaUint16Properties)+2+strlen(X.label2)+1)
#define LUA_STRING_SIZE(X) (uint8_t)(4+strlen(X.label1)+1+strlen(X.label2)+1)

/**we dont use this yet for OUR LUA
#define LUA_INT8_SIZE(X) 4+strlen(X.label1)+1+sizeof(tagLuaInt8Properties)+strlen(X.label2)+1
#define LUA_UINT16_SIZE(X) 4+strlen(X.label1)+1+sizeof(tagLuaUint16Properties)+strlen(X.label2)+1
#define LUA_INT16_SIZE(X) 4+strlen(X.label1)+1+sizeof(tagLuaInt16Properties)+strlen(X.label2)+1
#define LUA_FLOAT_SIZE(X) 4+strlen(X.label1)+1+sizeof(tagLuaFloatProperties)+strlen(X.label2)+1
*/

extern void sendLuaFieldCrsf(uint8_t idx, uint8_t chunk);

extern void suppressCurrentLuaWarning(void);
extern bool getLuaWarning(void);
extern void ICACHE_RAM_ATTR luaParamUpdateReq();
extern bool luaHandleUpdateParameter();

void registerLUAPopulateParams(void (*populate)());

typedef void (*luaCallback)(uint8_t id, uint8_t arg);
void registerLUAParameter(void *definition, luaCallback callback = 0);

void setLuaTextSelectionValue(struct tagLuaItem_textSelection *textSelectionStruct, uint8_t newvalue);
void setLuaCommandValue(struct tagLuaItem_command *textSelectionStruct, uint8_t newvalue);
void setLuaUint8Value(struct tagLuaItem_uint8 *luaStruct, uint8_t newvalue);
void setLuaUint16Value(struct tagLuaItem_uint16 *luaStruct, uint16_t newvalue);
void setLuaStringValue(struct tagLuaItem_string *luaStruct,const char *newvalue);
#endif

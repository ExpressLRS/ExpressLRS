#pragma once

#ifdef TARGET_TX

#include "targets.h"
#include "crsf_protocol.h"

#define LUA_DEVICE_SIZE(X) (uint8_t)(sizeof(tagLuaDeviceProperties)+strlen(X.label1)+1)
#define LUA_UINT8_SIZE(X) (uint8_t)(2+strlen(X.label1)+1+sizeof(tagLuaUint8Properties)+1+strlen(X.label2)+1)
#define LUA_UINT16_SIZE(X) (uint8_t)(2+strlen(X.label1)+1+sizeof(tagLuaUint16Properties)+2+strlen(X.label2)+1)
#define LUA_STRING_SIZE(X) (uint8_t)(2+strlen(X.label1)+1+strlen(X.label2)+1)
#define LUA_FOLDER_SIZE(X) (uint8_t)(2+strlen(X.label1)+1)

extern void sendLuaCommandResponse(struct luaItem_Command *cmd, uint8_t step, const char *message);

extern void suppressCurrentLuaWarning(void);
extern bool getLuaWarning(void);
extern void ICACHE_RAM_ATTR luaParamUpdateReq();
extern bool luaHandleUpdateParameter();

void registerLUAPopulateParams(void (*populate)());

typedef void (*luaCallback)(uint8_t id, uint8_t arg);
void registerLUAParameter(void *definition, luaCallback callback = 0, uint8_t parent = 0);

void sendLuaDevicePacket(void);
inline void setLuaTextSelectionValue(struct luaItem_Selection *luaStruct, uint8_t newvalue) {
    luaStruct->value = newvalue;
}
inline void setLuaUint8Value(struct tagLuaItem_uint8 *luaStruct, uint8_t newvalue){
    luaStruct->luaProperties2.value = newvalue;
}
inline void setLuaUint16Value(struct tagLuaItem_uint16 *luaStruct, uint16_t newvalue){
    luaStruct->luaProperties2.value = (newvalue >> 8) | (newvalue << 8);
}
inline void setLuaStringValue(struct tagLuaItem_string *luaStruct,const char *newvalue){
    luaStruct->label2 = newvalue;
}
#endif

#pragma once

#ifdef TARGET_TX

#include "targets.h"
#include "crsf_protocol.h"

extern void sendLuaCommandResponse(struct luaItem_command *cmd, uint8_t step, const char *message);

extern void suppressCurrentLuaWarning(void);
extern bool getLuaWarning(void);
extern void ICACHE_RAM_ATTR luaParamUpdateReq();
extern bool luaHandleUpdateParameter();

void registerLUAPopulateParams(void (*populate)());

typedef void (*luaCallback)(uint8_t id, uint8_t arg);
void registerLUAParameter(void *definition, luaCallback callback = 0, uint8_t parent = 0);

void sendLuaDevicePacket(void);
inline void setLuaTextSelectionValue(struct luaItem_selection *luaStruct, uint8_t newvalue) {
    luaStruct->value = newvalue;
}
inline void setLuaUint8Value(struct luaItem_int8 *luaStruct, uint8_t newvalue) {
    luaStruct->properties.u.value = newvalue;
}
inline void setLuaInt8Value(struct luaItem_int8 *luaStruct, int8_t newvalue) {
    luaStruct->properties.s.value = newvalue;
}
inline void setLuaUint16Value(struct luaItem_int16 *luaStruct, uint16_t newvalue) {
    luaStruct->properties.u.value = (newvalue >> 8) | (newvalue << 8);
}
inline void setLuaInt16Value(struct luaItem_int16 *luaStruct, int16_t newvalue) {
    luaStruct->properties.s.value = (newvalue >> 8) | (newvalue << 8);
}
inline void setLuaStringValue(struct luaItem_string *luaStruct, const char *newvalue) {
    luaStruct->value = newvalue;
}
#endif

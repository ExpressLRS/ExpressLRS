#pragma once

#ifdef TARGET_TX

#include "targets.h"
#include "crsf_protocol.h"

struct luaPropertiesCommon {
    const crsf_value_type_e type;
    uint8_t id;         // Sequential id assigned by enumeration
    uint8_t parent;     // id of parent folder
} PACKED;

struct tagLuaDeviceProperties {
    uint32_t serialNo;
    uint32_t hardwareVer;
    uint32_t softwareVer;
    uint8_t fieldCnt; //number of field of params this device has
}PACKED;

struct luaItem_selection {
    struct luaPropertiesCommon common;
    uint8_t value;
    const char* const name;    // display name
    const char* const options; // selection options, separated by ';'
    const char* const units;
} PACKED;

struct luaItem_command {
    struct luaPropertiesCommon common;
    uint8_t step;           // state
    const char* const name; // display name
    const char *info;       // status info to display
} PACKED;

struct luaPropertiesInt8 {
    union {
        struct {
            int8_t value;
            const int8_t min;
            const int8_t max;
        } s;
        struct {
            uint8_t value;
            const uint8_t min;
            const uint8_t max;
        } u;
    };
} PACKED;

struct luaItem_int8 {
    struct luaPropertiesCommon common;
    struct luaPropertiesInt8 properties;
    const char* const name;  // display name
    const char* const units;
} PACKED;

struct luaPropertiesInt16 {
    union {
        struct {
            int16_t value;
            const int16_t min;
            const int16_t max;
        } s;
        struct {
            uint16_t value;
            const uint16_t min;
            const uint16_t max;
        } u;
    };
}PACKED;

struct luaItem_int16 {
    struct luaPropertiesCommon common;
    struct luaPropertiesInt16 properties;
    const char* const name; // display name
    const char* const units;
} PACKED;

struct luaItem_string {
    const struct luaPropertiesCommon common;
    const char* const name; // display name
    const char* value;
} PACKED;

struct luaItem_folder {
    const struct luaPropertiesCommon common;
    const char* const name; // display name
} PACKED;

struct tagLuaElrsParams {
    uint8_t pktsBad;
    uint16_t pktsGood; // Big-Endian
    uint8_t flags;
    char msg[1]; // null-terminated string
} PACKED;

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

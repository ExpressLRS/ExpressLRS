#pragma once


#include "targets.h"
#include "crsf_protocol.h"

enum lua_Flags{
    //bit 0 and 1 are status flags, show up as the little icon in the lua top right corner
    LUA_FLAG_CONNECTED = 0,
    LUA_FLAG_STATUS1,
    //bit 2,3,4 are warning flags, change the tittle bar every 0.5s
    LUA_FLAG_MODEL_MATCH,
    LUA_FLAG_ISARMED,
    LUA_FLAG_WARNING1,
    //bit 5,6,7 are critical warning flag, block the lua screen until user confirm to suppress the warning.
    LUA_FLAG_ERROR_CONNECTED,
    LUA_FLAG_ERROR_BAUDRATE,
    LUA_FLAG_CRITICAL_WARNING2,
};

struct luaPropertiesCommon {
    const char* const name;    // display name
    crsf_value_type_e type;
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
    const char* options; // selection options, separated by ';'
    const char* units;
} PACKED;

enum luaCmdStep_e : uint8_t {
    lcsIdle = 0,
    lcsClick = 1,       // user has clicked the command to execute
    lcsExecuting = 2,   // command is executing
    lcsAskConfirm = 3,  // command pending user OK
    lcsConfirmed = 4,   // user has confirmed
    lcsCancel = 5,      // user has requested cancel
    lcsQuery = 6,       // UI is requesting status update
};

struct luaItem_command {
    struct luaPropertiesCommon common;
    luaCmdStep_e step;      // state
    const char *info;       // status info to display
} PACKED;

struct luaPropertiesInt8 {
    union {
        struct {
            uint8_t value;
            const uint8_t min;
            const uint8_t max;
        } u;
        struct {
            int8_t value;
            const int8_t min;
            const int8_t max;
        } s;
    };
} PACKED;

struct luaItem_int8 {
    struct luaPropertiesCommon common;
    struct luaPropertiesInt8 properties;
    const char* const units;
} PACKED;

struct luaPropertiesInt16 {
    union {
        struct {
            uint16_t value;
            const uint16_t min;
            const uint16_t max;
        } u;
        struct {
            int16_t value;
            const int16_t min;
            const int16_t max;
        } s;
    };
} PACKED;

struct luaItem_int16 {
    struct luaPropertiesCommon common;
    struct luaPropertiesInt16 properties;
    const char* const units;
} PACKED;

struct luaPropertiesFloat {
    // value, min, max, and def are all signed, but stored as BE unsigned
    uint32_t value;
    const uint32_t min;
    const uint32_t max;
    const uint32_t def; // default value
    const uint8_t precision;
    const uint32_t step;
} PACKED;

struct luaItem_float {
    struct luaPropertiesCommon common;
    struct luaPropertiesFloat properties;
    const char* const units;
} PACKED;

struct luaItem_string {
    struct luaPropertiesCommon common;
    const char* value;
} PACKED;

struct luaItem_folder {
    struct luaPropertiesCommon common;
    char* dyn_name;
} PACKED;

struct tagLuaElrsParams {
    uint8_t pktsBad;
    uint16_t pktsGood; // Big-Endian
    uint8_t flags;
    char msg[1]; // null-terminated string
} PACKED;

#ifdef TARGET_TX
void setLuaWarningFlag(lua_Flags flag, bool value);
uint8_t getLuaWarningFlags(void);

void luaRegisterDevicePingCallback(void (*callback)());
#endif

#define LUA_FIELD_HIDE(fld) { fld.common.type = (crsf_value_type_e)((uint8_t)fld.common.type | CRSF_FIELD_HIDDEN); }
#define LUA_FIELD_SHOW(fld) { fld.common.type = (crsf_value_type_e)((uint8_t)fld.common.type & ~CRSF_FIELD_HIDDEN); }

void sendLuaCommandResponse(struct luaItem_command *cmd, luaCmdStep_e step, const char *message);

extern void luaParamUpdateReq(uint8_t type, uint8_t index, uint8_t arg);
extern bool luaHandleUpdateParameter();

typedef void (*luaCallback)(struct luaPropertiesCommon *item, uint8_t arg);
void registerLUAParameter(void *definition, luaCallback callback = nullptr, uint8_t parent = 0);

uint8_t findLuaSelectionLabel(const void *luaStruct, char *outarray, uint8_t value);

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
    luaStruct->properties.u.value = htobe16(newvalue);
}
inline void setLuaInt16Value(struct luaItem_int16 *luaStruct, int16_t newvalue) {
    luaStruct->properties.u.value = htobe16((uint16_t)newvalue);
}
inline void setLuaFloatValue(struct luaItem_float *luaStruct, int32_t newvalue) {
    luaStruct->properties.value = htobe32((uint32_t)newvalue);
}
inline void setLuaStringValue(struct luaItem_string *luaStruct, const char *newvalue) {
    luaStruct->value = newvalue;
}

#define LUASYM_ARROW_UP "\xc0"
#define LUASYM_ARROW_DN "\xc1"

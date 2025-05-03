#pragma once

#include "targets.h"

#include "crsf_protocol.h"

struct propertiesCommon
{
    const char *name; // display name
    crsf_value_type_e type;
    uint8_t id;     // Sequential id assigned by enumeration
    uint8_t parent; // id of parent folder
} PACKED;

struct selectionParameter
{
    propertiesCommon common;
    uint8_t value;
    const char *options; // selection options, separated by ';'
    const char *units;
} PACKED;

enum commandStep_e : uint8_t
{
    lcsIdle = 0,
    lcsClick = 1,      // user has clicked the command to execute
    lcsExecuting = 2,  // command is executing
    lcsAskConfirm = 3, // command pending user OK
    lcsConfirmed = 4,  // user has confirmed
    lcsCancel = 5,     // user has requested cancel
    lcsQuery = 6,      // UI is requesting status update
};

struct commandParameter
{
    propertiesCommon common;
    commandStep_e step; // state
    const char *info;   // status info to display
} PACKED;

struct int8Parameter
{
    propertiesCommon common;
    union {
        struct
        {
            uint8_t value;
            const uint8_t min;
            const uint8_t max;
        } u;
        struct
        {
            int8_t value;
            const int8_t min;
            const int8_t max;
        } s;
    } PACKED properties;
    const char *const units;
} PACKED;

struct int16Parameter
{
    propertiesCommon common;
    union {
        struct
        {
            uint16_t value;
            const uint16_t min;
            const uint16_t max;
        } u;
        struct
        {
            int16_t value;
            const int16_t min;
            const int16_t max;
        } s;
    } PACKED properties;
    const char *const units;
} PACKED;

struct floatParameter
{
    propertiesCommon common;
    struct
    {
        // value, min, max, and def are all signed, but stored as BE unsigned
        uint32_t value;
        const uint32_t min;
        const uint32_t max;
        const uint32_t def; // default value
        const uint8_t precision;
        const uint32_t step;
    } PACKED properties;
    const char *const units;
} PACKED;

struct stringParameter
{
    propertiesCommon common;
    const char *value;
} PACKED;

struct folderParameter
{
    propertiesCommon common;
    char *dyn_name;
} PACKED;

struct elrsStatusParameter
{
    uint8_t pktsBad;
    uint16_t pktsGood; // Big-Endian
    uint8_t flags;
    char msg[1]; // null-terminated string
} PACKED;

#define LUA_FIELD_HIDE(fld)                                                                  \
    {                                                                                        \
        fld.common.type = (crsf_value_type_e)((uint8_t)fld.common.type | CRSF_FIELD_HIDDEN); \
    }
#define LUA_FIELD_SHOW(fld)                                                                   \
    {                                                                                         \
        fld.common.type = (crsf_value_type_e)((uint8_t)fld.common.type & ~CRSF_FIELD_HIDDEN); \
    }
#define LUA_FIELD_VISIBLE(fld, cond) \
    {                                \
        if (cond)                    \
            LUA_FIELD_SHOW(fld)      \
        else                         \
            LUA_FIELD_HIDE(fld)      \
    }

typedef void (*parameterHandlerCallback)(propertiesCommon *item, uint8_t arg);

uint8_t findSelectionLabel(const selectionParameter *luaStruct, char *outArray, uint8_t value);

inline void setLuaTextSelectionValue(selectionParameter *luaStruct, const uint8_t newValue)
{
    luaStruct->value = newValue;
}
inline void setLuaUint8Value(int8Parameter *luaStruct, const uint8_t newValue)
{
    luaStruct->properties.u.value = newValue;
}
inline void setLuaInt8Value(int8Parameter *luaStruct, const int8_t newValue)
{
    luaStruct->properties.s.value = newValue;
}
inline void setLuaUint16Value(int16Parameter *luaStruct, const uint16_t newValue)
{
    luaStruct->properties.u.value = htobe16(newValue);
}
inline void setLuaInt16Value(int16Parameter *luaStruct, const int16_t newValue)
{
    luaStruct->properties.u.value = htobe16((uint16_t)newValue);
}
inline void setLuaFloatValue(floatParameter *luaStruct, const int32_t newValue)
{
    luaStruct->properties.value = htobe32((uint32_t)newValue);
}
inline void setLuaStringValue(stringParameter *luaStruct, const char *newValue)
{
    luaStruct->value = newValue;
}

#define LUASYM_ARROW_UP "\xc0"
#define LUASYM_ARROW_DN "\xc1"

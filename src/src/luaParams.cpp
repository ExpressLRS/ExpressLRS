
#include "luaParams.h"
struct tagLuaDevice luaDevice = {
    "ELRS",
    {
        {0},
        LUA_FIELD_AMOUNT
    },
    LUA_DEVICE_SIZE(luaDevice)
};


struct tagLuaItem_textSelection luaAirRate = {
    {
        1,
        0,
        0,
        CRSF_TEXT_SELECTION
    },
    "Pkt.Rate",
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) 
    "500(x);250(x);200(-112dbm);150(x);100(-117dbm);50(-120dbm);25(-123dbm)",
#elif defined(Regulatory_Domain_ISM_2400)
    "500(-105dbm);250(-108dbm);200(x);150(-112dbm);100(x);50(-117dbm);25(-120dbm)",
#endif
    {
        0,
        0,
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) 
        7,//4
#elif defined(Regulatory_Domain_ISM_2400)
        7,//5
#endif    
        0
    },
    "Hz",
    LUA_TEXTSELECTION_SIZE(luaAirRate)
};
struct tagLuaItem_textSelection luaTlmRate = {
    {
        2,
        0,
        0,
        CRSF_TEXT_SELECTION
    },
    "Pkt.Rate",
    "off;1/128;1/64;1/32;1/16;1/8;1/4;1/2",
    {
        0,
        0,
        7,    
        0
    },
    "Pkt",
    LUA_TEXTSELECTION_SIZE(luaTlmRate)
};
struct tagLuaItem_textSelection luaPower = {
    {
        3,
        0,
        0,
        CRSF_TEXT_SELECTION
    },
    "Power",
    "10;25;50;100;250;500;1000;2000",
    {
        0,
        0,
        7,    
        0
    },
    "mW",
    LUA_TEXTSELECTION_SIZE(luaPower)
};

struct tagLuaItem_command luaBind = {
    {
        4,
        0,
        0,
        CRSF_COMMAND
    },
    "Bind",
    {
        0,
        200
    },
    "rdy",
    LUA_COMMAND_SIZE(luaBind)
};
struct tagLuaItem_command luaWebUpdate = {
    {
        5,
        0,
        0,
        CRSF_COMMAND
    },
    "webUpdate",
    {
        0,
        200
    },
    "rdy",
    LUA_COMMAND_SIZE(luaWebUpdate)
};


void setLuaTextSelectionValue(struct tagLuaItem_textSelection *luaStruct, uint8_t newvalue){
    struct tagLuaItem_textSelection *p1 = (struct tagLuaItem_textSelection*)luaStruct;
    p1->luaProperties2.value = newvalue;
}
void setLuaCommandValue(struct tagLuaItem_command *luaStruct, uint8_t newvalue){
    struct tagLuaItem_command *p1 = (struct tagLuaItem_command*)luaStruct;
    p1->luaProperties2.status = newvalue;
}

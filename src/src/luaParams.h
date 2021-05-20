#pragma once
//#include "common.h"
#include "crsf_protocol.h"
//#include "CRSF.h"
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
        4,
#elif defined(Regulatory_Domain_ISM_2400)
        5,
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
        8,    
        0
    },
    "Pkt",
    LUA_TEXTSELECTION_SIZE(luaAirRate)
};


#include "luaParams.h"

const char thisCommit[] = {LATEST_COMMIT};
const struct tagLuaDevice luaDevice = {
    "ELRS",
    {{0},LUA_FIELD_AMOUNT},
    LUA_DEVICE_SIZE(luaDevice)
};


struct tagLuaItem_textSelection luaAirRate = {
    {1,0,0,(uint8_t)CRSF_TEXT_SELECTION}, //id,chunk,parent,type
    "P.Rate",
#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433) 
    "200(-112dbm);100(-117dbm);50(-120dbm);25(-123dbm)",
#elif defined(Regulatory_Domain_ISM_2400)
    "500(-105dbm);250(-108dbm);150(-112dbm);50(-117dbm)",
#endif
    {0,0,3,0},//value,min,max,default
    "Hz",
    LUA_TEXTSELECTION_SIZE(luaAirRate)
};
struct tagLuaItem_textSelection luaTlmRate = {
    {2,0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,chunk,parent,type
    "T.Rate",
    "off;1/128;1/64;1/32;1/16;1/8;1/4;1/2",
    {0,0,7,0},//value,min,max,default
    " ",
    LUA_TEXTSELECTION_SIZE(luaTlmRate)
};
struct tagLuaItem_textSelection luaPower = {
    {3,0,0,(uint8_t)CRSF_TEXT_SELECTION},//id,chunk,parent,type
    "Pwr",
    "10;25;50;100;250;500;1000;2000",
    {0,0,7,0},//value,min,max,default
    "mW",
    LUA_TEXTSELECTION_SIZE(luaPower)
};

struct tagLuaItem_command luaBind = {
    {4,0,0,(uint8_t)CRSF_COMMAND},//id,chunk,parent,type
    "Bind",
    {0,200},//status,timeout
    " ",
    LUA_COMMAND_SIZE(luaBind)
};
struct tagLuaItem_command luaWebUpdate = {
    {5,0,0,(uint8_t)CRSF_COMMAND},//id,chunk,parent,type
    "Update",
    {0,200},//status,timeout
    " ",
    LUA_COMMAND_SIZE(luaWebUpdate)
};

struct tagLuaItem_uint8 luaBadPkt = {
    {6,0,0,(uint8_t)CRSF_UINT8},//id,chunk,parent,type
    "Bad",
    {0,0,254,0},//value,min,max,default
    " ",
    LUA_UINT8_SIZE(luaBadPkt)
};
struct tagLuaItem_uint16 luaGoodPkt = {
    {7,0,0,(uint8_t)CRSF_UINT16},//id,chunk,parent,type
    "Good",
    {0,0,511,0},//value,min,max,default
    " ",
    LUA_UINT16_SIZE(luaGoodPkt)
};

struct tagLuaItem_string luaCommit = {
    {8,0,0,(uint8_t)CRSF_STRING},//id,chunk,parent,type
    thisCommit,
    " ",
    LUA_STRING_SIZE(luaCommit)
};


void setLuaTextSelectionValue(struct tagLuaItem_textSelection *luaStruct, uint8_t newvalue){
    struct tagLuaItem_textSelection *p1 = (struct tagLuaItem_textSelection*)luaStruct;
    p1->luaProperties2.value = newvalue;
}
void setLuaCommandValue(struct tagLuaItem_command *luaStruct, uint8_t newvalue){
    struct tagLuaItem_command *p1 = (struct tagLuaItem_command*)luaStruct;
    p1->luaProperties2.status = newvalue;
}
void setLuaUint8Value(struct tagLuaItem_uint8 *luaStruct, uint8_t newvalue){
    struct tagLuaItem_uint8 *p1 = (struct tagLuaItem_uint8*)luaStruct;
    p1->luaProperties2.value = newvalue;
}
void setLuaUint16Value(struct tagLuaItem_uint16 *luaStruct, uint16_t newvalue){
    struct tagLuaItem_uint16 *p1 = (struct tagLuaItem_uint16*)luaStruct;
    p1->luaProperties2.value = (newvalue >> 8) | (newvalue << 8);
}
/**
void setLuaCommandInfo(struct tagLuaItem_command *luaStruct, const char *newvalue){
    struct tagLuaItem_command *p1 = (struct tagLuaItem_command*)luaStruct;
    p1->label2 = newvalue;
}
 */
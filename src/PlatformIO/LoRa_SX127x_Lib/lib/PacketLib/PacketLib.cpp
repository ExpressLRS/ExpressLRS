#include "PacketLib.h"



MASTERorSLAVE_ PacketLib::MASTERorSLAVE = MASTER;


void PacketLib::ProcessPacket(){
}

void PacketLib::InitContact(){

    if(MASTERorSLAVE==MASTER){

        MasterMakeFirstContact();

    }else{

        SlaveMakeFirstContact();

    }
}

void PacketLib::MasterMakeFirstContact(){

}


void PacketLib::SlaveMakeFirstContact(){

}

void PacketLib::BeginBindingMaster(){

}

void PacketLib::BeginBindingSlave(){
    
}

uint8_t PacketLib:: crc8(const uint8_t * ptr, uint8_t len){
    uint8_t crc = 0;
    for (uint8_t i=0; i<len; i++) {
    crc = crc8tab[crc ^ *ptr++];
    }
    return crc;
}
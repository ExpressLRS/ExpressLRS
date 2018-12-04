#include "CRSF.h"
#include "PacketLib.h"
#include <Arduino.h>

HardwareSerial CRSF::Port = NULL;

uint8_t CRSF::CSFR_TXpin = 2;
uint8_t CRSF::CSFR_RXpin = 2;  // Same pin for RX/TX



void CRSF::Init(HardwareSerial SerialPort){

    CRSF::Port = SerialPort; /// Save to class variable so we know which port to use for future operations. 

    if ((PacketLib::MASTERorSLAVE)==MASTER){

        Port.begin(CRSF_OPENTX_BAUDRATE, SERIAL_8N1);

    }else{

        Port.begin(CRSF_RX_BAUDRATE, SERIAL_8N1);
    }

}


void CRSF::ReadSerialData(){

}    

void CRSF::WriteSerialData(){

}


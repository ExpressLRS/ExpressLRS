#ifndef H_PACKETLIB
#define H_PACKETLIB

#include "LoRa_lowlevel.h"
#include "LoRa_SX127x.h"
#include "LoRa_SX1276.h"
#include "LoRa_SX1278.h"


typedef enum {COMM_NULL, COMM_ESTABLISHED, COMM_MASTER_BINDMODE, COMM_SLAVE_BINDMODE, COMM_LOST, COMM_MASTER_FIRSTCONTACT, COMM_SLAVE_FIRSTCONTACT} COMM_STATE_;

typedef enum {MASTER, SLAVE} MASTERorSLAVE_; 




class PacketLib{


    public:

        static float CRCerrRate();
        static float PacketLossRate();

        static uint32_t CRCerrors;
        static uint32_t LostPackets;

        static uint32_t FailSafeAfter; //failsafe should be considered active after this many seconds of no contact

        static COMM_STATE_ COMM_STATE;
        static MASTERorSLAVE_ MASTERorSLAVE;

        static uint8_t nonce;
        static uint8_t flags[2];

        static uint8_t crc8(const uint8_t * ptr, uint8_t len);

        static void ProcessPacket();
        static void DecodePacket();
        static void PacketToChanData();

        static void CheckIfAirRateChangeReq(); //check if nessesary to change config to lower bandwidth etc
        static void CheckIfPwrChangeReq(); //check if nessesary to increase/decrease output power

        static void BeginBindingMaster(); //Begin binding process master
        static void BeginBindingSlave(); //Begin process for binding slave

        static void MasterMakeFirstContact();
        static void SlaveMakeFirstContact();

        static void InitContact(); //First initiation of contact with the other module. 


};

#endif
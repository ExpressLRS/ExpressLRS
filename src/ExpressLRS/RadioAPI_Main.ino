#include "RadioAPI.h"
/////////////Packet Stats//////////
extern int8_t LastPacketRSSI;
extern float LastPacketSNR;
extern float PacketLossRate;
//////////////////////////////////

//////////////Hardware Settings//
extern RadioRXTXmode RXTXmode;
extern RFmodule_ RFmodule;
/////////////////////////////////

extern uint32_t TXstartMicros;
extern uint32_t LastTXdoneMicros;

extern char TXbuff[256];
extern char RXbuff[256];

unsigned long CRCerrors;
unsigned long LostPackets;

byte RXflags;
byte TXflags;


byte LastNonceRX;
byte nonceRX;
byte nonceTX;

////////////////////////////////////

byte nonce = 0;
byte flags = 0;


void detectRFmodType() {
  RFmodule = RFMOD_SX1278;

}

void ProcessPacketIn(char data, int length) {
  //char inCRC = data[length];
  // byte flags = data[0];
}



void IRAM_ATTR SendRFPacket(char* data, uint8_t length) {

  nonce = nonce + 1;
  byte PacketLen;

  for (int i = 2; i  <= length + 2; i++) {
    TXbuff[i] = data[i];
  }

  //flags header
  TXbuff[0] = flags;

  // add nonce
  TXbuff[length + 1] = nonce;

  byte crc = 0x11;
  TXbuff[length + 2] = crc;


  //// Actually Send to Radio ////

  if (RFmodule = RFMOD_SX1278) {
    SX127xTXnb(TXbuff, length + 3);
  } else {
    SX127xTXnb(TXbuff, length + 3);
  }
}

void IRAM_ATTR decodePacketIn(char* data, uint8_t length) {
  //calc crc for packet
  uint8_t CRCin = RXbuff[length];
  uint8_t CRCcalc = calcCRC8(RXbuff, 2, length - 1);

  if (!CRCin == CRCcalc) {
    CRCerrors = CRCerrors + 1;
  } else {

    RXflags = RXbuff[0];
    nonceRX  = RXbuff[1];

    if !((LastNonceRX + 1) = nonceRX) {
      LostPackets = LostPackets + (nonceRX - LastNonce);
    }
    //Process flags

  }


  //grab data part and decode into packet data

}


void scanSpectrum(float startFreq, float stepSize, float endFreq) {
  //reset sx128x to put info fsk mode?

}

unsigned short calcCRC16(const unsigned char* data_p, unsigned char length) {
  unsigned char x;
  unsigned short crc = 0xFFFF;

  while (length--) {
    x = crc >> 8 ^ *data_p++;
    x ^= x >> 4;
    crc = (crc << 8) ^ ((unsigned short)(x << 12)) ^ ((unsigned short)(x << 5)) ^ ((unsigned short)x);
  }
  return crc;
}


uint8_t calcCRC8(char* data , uint8_t startADR, uint8_t endADR) {
  return 0x01;
  //todo make crc actually work
}

void RFmoduleBegin() {
  if (RFmodule == RFMOD_SX1278) {
    SX1278begin();
  } else {
    SX1276begin();
  }
}




extern int8_t LastPacketRSSI;
extern float LastPacketSNR;
extern float PacketLossRate;

byte nonce = 0;
byte flags = 0;


enum RFmodules {RFMOD_SX1276, RFMOD_SX1278};
RFmodules RFmodule;

void detectRFmodType() {
  RFmodule = RFMOD_SX1278;

}

void SendRFPacket(char* data, uint8_t length) {

  nonce = nonce + 1;
  char OutData[255];
  byte PacketLen;

  for (int i = 1; i  <= length+1; i++) {
    OutData[i] = data[i];
  }

  //flags header
  OutData[0] = flags;

  // add nonce
  OutData[length + 1] = nonce;

  byte crc = 0x11;
  OutData[length + 2] = crc;



  if (RFmodule = RFMOD_SX1278) {
    SX127xTXnb(OutData, length + 3);
  }


  if (RFmodule = RFMOD_SX1276) {

    SX127xTXnb(OutData, length + 3);

  }
}


void scanSpectrum(float startFreq, float stepSize, float endFreq){
  
}


////////////////Functions for TimerEnginer and Timeout Calculations////////////////////



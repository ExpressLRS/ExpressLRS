#include <SBUS.h>

extern char RCdataIN[numRCchannels];
extern char RCdataOUT[numRCchannels];
extern char RXbuff[256];
extern RFdataMode_ RFdataMode;

int ChannelData[16];
#define RCinPin 16
#define RCoutPin 17

//https://github.com/mikeshub/FUTABA_SBUS/tree/master/FUTABA_SBUS/examples/sbus_example
// https://github.com/zendes/SBUS/blob/master/SBUS.cpp


#if defined(ARDUINO_ESP32_DEV)
SBUS sbus(Serial2);
#else
SBUS sbus(Serial);
#endif




char serBuffer[25];
uint16_t SBUSchannels[16];

void beginSBUS() {

  //#if PLATFORM == ESP
  //sbus.begin(100000, SERIAL_8N2, RCinPin, RCoutPin, true);
  //#else
  sbus.begin();
  //#endif

}

void SBUSprocess() {
  sbus.process();
  if (sbus.isPacketReady()) {
    printSBUSStatus();
    SBUStoRCdata10Bit();
    SX127xTXnb(RCdataOUT, 6);
  }
}

void RCdatatoSBUS() {

}

void SBUStoRCdata10Bit() {

  for (int i = 0; i < 4; i ++) {
    RCdataIN[i] = sbus.mapCh10Bit(i + 1);
  }
  RCdataIN[4] = (sbus.mapCh10Bit(4) >> 8) + ((sbus.mapCh10Bit(3) >> 6) & 0b00001100) + ((sbus.mapCh10Bit(2) >> 4) & 0b00110000) + ((sbus.mapCh10Bit(1) >> 2) & 0b11000000);
  RCdataIN[5] = (sbus.mapCh2Bit(5) << 6) + ((sbus.mapCh2Bit(6) << 4) & 0b00110000) + ((sbus.mapCh2Bit(7) << 2) & 0b00001100) + (sbus.mapCh2Bit(8) & 0b00000011);
}

void printSBUSStatus() {

  //  Serial.print("Time diff: ");
  //  Serial.println(sbus.getLastTime());
  //    Serial.print(sbus.mapCh10Bit(1));
  //    Serial.print(":");
  //    Serial.print(sbus.mapCh10Bit(2));
  //    Serial.print(":");
  //    Serial.print(sbus.mapCh10Bit(3));
  //    Serial.print(":");
  //    Serial.print(sbus.mapCh10Bit(4));
  //    Serial.println(":");
  //  Serial.print("Ch5  ");
  //  Serial.println(sbus.getNormalizedChannel(5));
  //  Serial.print("Ch6  ");
  //  Serial.println(sbus.getNormalizedChannel(6));
  //  Serial.print("Ch7  ");
  //  Serial.println(sbus.getNormalizedChannel(7));
  //  Serial.print("Ch8  ");
  //  Serial.println(sbus.getNormalizedChannel(8));
  //  Serial.print("Ch9  ");
  //  Serial.println(sbus.getNormalizedChannel(9));
  //  Serial.print("Ch10 ");
  //  Serial.println(sbus.getNormalizedChannel(10));
  //  Serial.print("Ch11 ");
  //  Serial.println(sbus.getNormalizedChannel(11));
  //  Serial.print("Ch12 ");
  //  Serial.println(sbus.getNormalizedChannel(12));
  //  Serial.print("Ch13 ");
  //  Serial.println(sbus.getNormalizedChannel(13));
  //  Serial.print("Ch14 ");
  //  Serial.println(sbus.getNormalizedChannel(14));
  //  Serial.print("Ch15 ");
  //  Serial.println(sbus.getNormalizedChannel(15));
  //  Serial.print("Ch16 ");
  //  Serial.println(sbus.getNormalizedChannel(16));
  //  Serial.println();
  //  Serial.print("Failsafe: ");
  //  if (sbus.getFailsafeStatus() == SBUS_FAILSAFE_ACTIVE) {
  //    Serial.println("Active");
  //  }
  //  if (sbus.getFailsafeStatus() == SBUS_FAILSAFE_INACTIVE) {
  //    Serial.println("Not Active");
  //  }
  //
  //  Serial.print("Data loss on connection: ");
  //  Serial.print(sbus.getFrameLoss());
  //  Serial.println("%");
  //
  //  Serial.print("Frames: ");
  //  Serial.print(sbus.getGoodFrames());
  //  Serial.print(" / ");
  //  Serial.print(sbus.getLostFrames());
  //  Serial.print(" / ");
  //  Serial.println(sbus.getDecoderErrorFrames());

}

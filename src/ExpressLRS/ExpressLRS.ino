#include "Module.h"
#include "SX127x.h"

char buff[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

bool ModuleMaster = true;


void setup() {

  Serial.begin(115200);
  Serial.println("booting....");
  Serial.println(SX1278begin());
  Serial.println(SX1278config(BW_500_00_KHZ, SF_7, CR_4_5, 434000000, SX127X_SYNC_WORD));
  SX1278rxCont();
  beginSBUS();
  delay(500);
  WifiAP();
  delay(500);
  initWebSever();
  delay(500);

  Serial.begin(115200);
  //SerialBT.begin("ESP32test"); //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");


}

void loop() {
  
  //doBleSerial();
  
  HandleWebserver();

  if (ModuleMaster) {
    SBUSprocess();
  }


  //yield();
  //SBUSprocess();
  //delay(12);
  //SX127xTXnb(RCdataOut, 6);


  //SX127xTX(buff, 9);
  //  if (radioIdle == true) {
  //    Serial.println(SX127xTXnb(buff, 9));
  //    Serial.println("RadioIdle");
  //  } else {
  //    Serial.println("RadioNotIdle");
  //    delay(100);
  //  }

}


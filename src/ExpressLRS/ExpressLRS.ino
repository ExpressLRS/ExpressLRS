//#include "RadioAPI_lowlevel.h"
//#include "SX127x.h"
#include "RadioAPI.h"


//////////////////////////////////////////// Webserver
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
//#include <Hash.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SPIFFSEditor.h>
#include <AsyncTCP.h>

#include <ESPAsyncWebServer.h>

// SKETCH BEGIN
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
AsyncEventSource events("/events");
//////////////////////////////////////////////


char TestData[6] = {1, 2, 3, 4, 5, 6};

char buff[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

bool ModuleMaster = true;
bool radioIdle = true;

bool startup = true;

unsigned long lastTXtime = 0;
unsigned long txDelay = 2000;


//hw_timer_t * timer = NULL;
//portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//#include "BluetoothSerial.h"
//BluetoothSerial SerialBT;

void IRAM_ATTR onTimer() {
  SX127xTXnb(TestData, 6);
}

void setup() {

  Serial.begin(115200);
  Serial.println("booting ExpressLRS module....");
  EEMPROMinit();
  ReadEEPROMvars();
  ConfigureHardware();
  RFmoduleBegin();
  InitRC();

  //Serial.println(SX1278begin());
  Serial.println(SX1276config(BW_500_00_KHZ, SF_7, CR_4_5, 915000000, SX127X_SYNC_WORD));
  //attachInterrupt(26, SX127xTXnbISR, RISING);

#if defined(ARDUINO_ESP32_DEV)


  delay(500);
 // WifiAP();
  delay(500);
  initWebSever();
  delay(500);

#endif
  // SerialBT.begin("ESP32test"); //Bluetooth device name
  //Serial.println("The device started, now you can pair it with bluetooth!");


 
  //    //timerAlarmEnable(timer);
}




void loop() {

  //doBleSerial();
  //
  if (startup) {
    delay(100);
    attachInterrupt(digitalPinToInterrupt(26), SX127xTXnbISR, RISING);
    //SX127xTXnb(TestData, 6);
    //timerAlarmEnable(timer);
    StartTX();
    startup = false;
  }
  delay(100);
  //delay(12);
  //onTimer();

  //HandleWebserver();
  //ProcessRC();
  //ProcessLed();
  yield();
  //SBUSprocess();
  //if (millis() > (lastTXtime + txDelay)) {
  //(SX127xTXnb(TestData, 6));
  //SX127xRXnb();
  //lastTXtime = millis();
  //}

  //SX127xTX(buff, 9);
  //  if (radioIdle) {
  //    SX127xTXnb(TestData, 6);
  //  }
  //}
  //    Serial.println(SX127xTXnb(buff, 9));
  //    Serial.println("RadioIdle");
  //  } else {
  //    Serial.println("RadioNotIdle");
  //    delay(100);
  //  }

}


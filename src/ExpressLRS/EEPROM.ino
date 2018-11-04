///Add EEPROM FUNCTIONS

int addr = 0;
#define EEPROM_SIZE 64

#include "EEPROM.h"

////////////////EEPROM ADDRESS LIST/////////
#define EEPROMaddrDebugVerbosity 0x00
#define EEPROMaddrSerialRXmode 0x01
#define EEPROMaddrTrainerMode 0x02
#define EEPROMaddrSerialTXmode 0x03
#define EEPROMaddrSerialTelmMode 0x04
#define EEPROMaddrBleMode 0x05
#define EEPROMaddrRXTXmode 0x06
#define EEPROMaddrRFdataMode 0x07
#define EEPROMaddrRFmodule 0x08
#define EEPROMaddrPlatform 0x09
////////////////////////////////////////////

enum Verbosity_ {DEBUG_1, DEBUG_2, DEBUG_3, DEBUG_4};
enum RadioRXTXmode {RADIO_TX_MASTER, RADIO_RX_SLAVE};
enum BaudRates {SER_2400, SER_4800, SER_9600, SER_57600, SER_115200};
enum SerialRXmode {PROTO_RX_NONE, PROTO_RX_SBUS, PROTO_RX_PXX, PROTO_RX_CRSF};
enum TrainerMode {TRN_None, PROTO_TRN_PPM, PROTO_TRN_SBUS};
enum SerialTXmode {TX_None, PROTO_TX_SBUS, PROTO_TX_PXX, PROTO_TX_CRSF};
enum SerialTelmMode {TLM_None, PROTO_TLM_SPORT, PROTO_TLM_CRSF};
enum BluetoothMode {BLE_OFF, BLE_MIRROR_TLM, BLE_SER_DBG};
enum Platform_ {PLATFORM_ESP32, PLATFORM_ESP8266};

enum RFdataMode_ {RF_DATA_4CH, RF_DATA_4_4CH, RF_DATA_8CH};
enum RFmodule_ {RFMOD_SX1278, RFMOD_SX1276};

SerialRXmode SerRXmode = PROTO_RX_SBUS;
TrainerMode TrainTXmode = PROTO_TRN_SBUS;
SerialTXmode SerTXmode = PROTO_TX_SBUS;
SerialTelmMode SerTelmMode = PROTO_TLM_SPORT;
BluetoothMode BleMode = BLE_OFF;
RadioRXTXmode RXTXmode = RADIO_TX_MASTER;
RFdataMode_ RFdataMode = RF_DATA_4_4CH;
RFmodule_ RFmodule = RFMOD_SX1276;
Platform_ Platform = PLATFORM_ESP32;

Verbosity_ DebugVerbosity = DEBUG_2;


char MACaddr[6] = {0, 0, 0, 0, 0, 0};


void EEMPROMinit() {

#if defined(ARDUINO_ESP32_DEV)
  if (!EEPROM.begin(EEPROM_SIZE))
  {
    Serial.println("failed to initialise EEPROM, fatal error"); delay(1000000);
  }

#else
  EEPROM.begin(EEPROM_SIZE);
#endif
}

void getMACaddrEEPROM() { //get MAC address of module

}

void ReadEEPROMvars() {
  Serial.println("Reading values from EEPROM");
  //take data out of EEPROM and store it in RAM

  SerRXmode = (SerialRXmode)EEPROM.read(EEPROMaddrSerialRXmode);
  Serial.print("SerRXmode: ");
  Serial.println(SerRXmode);


  TrainTXmode = (TrainerMode)EEPROM.read(EEPROMaddrTrainerMode);
  Serial.print("TrainTXmode: ");
  Serial.println(TrainTXmode);

  SerTXmode = (SerialTXmode)EEPROM.read(EEPROMaddrSerialTXmode);
  Serial.print("SerTXmode: ");
  Serial.println(SerTXmode);


  SerTelmMode = (SerialTelmMode)EEPROM.read(EEPROMaddrSerialTelmMode);
  Serial.print("SerTelmMode: ");
  Serial.println(SerTelmMode);


  BleMode = (BluetoothMode)EEPROM.read(EEPROMaddrSerialTelmMode);
  Serial.print("BleMode: ");
  Serial.println(BleMode);

}

void SaveEEPROMvars() {
  Serial.println("Writing values to EEPROM");

  Serial.print("SerRXmode: ");
  Serial.println(SerRXmode);
  EEPROM.write(EEPROMaddrSerialRXmode, (byte)SerRXmode);

  Serial.print("TrainTXmode: ");
  Serial.println(TrainTXmode);
  EEPROM.write(EEPROMaddrTrainerMode, (byte)TrainTXmode);

  Serial.print("SerTXmode: ");
  Serial.println(SerTXmode);
  EEPROM.write(EEPROMaddrSerialTXmode, (byte)SerTXmode);

  Serial.print("SerTelmMode: ");
  Serial.println(SerTelmMode);
  EEPROM.write(EEPROMaddrSerialTelmMode, (byte)SerTelmMode);

  Serial.print("BleMode: ");
  Serial.println(BleMode);
  EEPROM.write(EEPROMaddrBleMode, (byte)BleMode);

  EEPROM.commit();

  delay(100);
}




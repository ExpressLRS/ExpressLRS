
#if defined(ARDUINO_ESP32_DEV)
/////////////////////////////////////////////////////
#define nss 5
#define dio0 26
#define dio1 25

byte fanPin = 22;
byte enable5VregPin = 39;
byte TXenablePin = 12;
byte RXenablePin = 13;

byte BuzPin = 15;
byte LowPowerPin = 21;

byte HeartbeatPin = 39;
byte VBATsense = 34;

int VBATdetectionInterval = 1000;
int VBATthreshold = 500;

byte LEDpin = 27;
byte SwitchPin = 36;
//////////////////////////////////////////////////////
#else
//////////////////////////////////////////////////////
#define nss 15
#define dio0 16
#define dio1 5
byte SwitchPin = 0;
byte LEDpin = 4;
/////////////////////////////////////////////////////
#endif





void ConfigureHardware() {
  ConfigHardwarePins();
  MeasureVBAT();
}


void ConfigHardwarePins() {

#if defined(ARDUINO_ESP32_DEV)

  //outputs
  pinMode(LEDpin, OUTPUT);
  pinMode(fanPin, OUTPUT);
  pinMode(enable5VregPin, OUTPUT);
  pinMode(TXenablePin, OUTPUT);
  pinMode(RXenablePin, OUTPUT);
  pinMode(BuzPin, OUTPUT);
  pinMode(LowPowerPin, OUTPUT);

  //inputs
  pinMode(SwitchPin, INPUT);
  pinMode(HeartbeatPin, INPUT);

#else

  //outputs
  pinMode(LEDpin,OUTPUT);

  //inputs
  pinMode(SwitchPin, INPUT);
  pinMode(dio0, INPUT);
  pinMode(dio1, INPUT);
  digitalWrite(LEDpin,HIGH);
  delay(500);
  digitalWrite(LEDpin,LOW);
#endif

}

#if defined(ARDUINO_ESP32_DEV)

void MeasureVBAT() {
  analogRead(VBATsense);
  Serial.print("VBAT: ");
  Serial.println(VBATsense);
  //map() to correct voltage blah blah
}

#else

void MeasureVBAT() {
  return;
}

#endif

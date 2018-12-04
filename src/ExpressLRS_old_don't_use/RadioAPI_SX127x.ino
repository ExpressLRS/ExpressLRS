#include "SX127x.h"
#define DEBUG

static SemaphoreHandle_t timer_sem;

extern byte FHSScounter;

hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

bool pinstate = false;

char TXbuff[256];
char RXbuff[256];

bool ledvalue = false;

bool polldio0; //poll dio0 because I hooked up the wrong pin in the hardware

extern Verbosity_ DebugVerbosity;
extern unsigned int TimeOnAir;

extern byte TXenablePin;

byte RXbuffLen = 255;
byte TXbuffLen = 255;
extern bool headerExplMode = false;

uint32_t TXstartMicros;
uint32_t LastTXdoneMicros;
extern bool radioIdle;


///////////Packet Statistics//////////
int8_t LastPacketRSSI = 0;
float LastPacketSNR = 0;
float PacketLossRate = 0;
////////////////////////////////////


uint8_t SX127xsetBandwidth(Bandwidth bw) {
  uint8_t state = SX127xconfig(bw, _sf, _cr, _freq, _syncWord);
  if (state == ERR_NONE) {
    _bw = bw;
  }
  return (state);
}

uint8_t SX127xsetSyncWord(uint8_t syncWord) {

  uint8_t status = setRegValue(SX127X_REG_SYNC_WORD, syncWord);
  if (status != ERR_NONE) {
    return (status);
  }
}

uint8_t SX127xSetOutputPower(uint8_t Power) {
  //todo make function turn on PA_BOOST ect
  uint8_t status = setRegValue(SX127X_REG_PA_CONFIG, 0b00000000, 3, 0);

  if (status != ERR_NONE) {
    return (status);
  }
}

uint8_t SX127xsetPreambleLength(uint8_t PreambleLen) {
  //status = setRegValue(SX127X_REG_PREAMBLE_MSB, SX127X_PREAMBLE_LENGTH_MSB);
  uint8_t status = setRegValue(SX127X_REG_PREAMBLE_LSB, PreambleLen);
  if (status != ERR_NONE) {
    return (status);
  }
}


uint8_t SX127xsetSpreadingFactor(SpreadingFactor sf) {
  uint8_t status;
  if (sf == SX127X_SF_6) {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
  } else {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
  }
  if (status == ERR_NONE) {
    _sf = sf;
  }
  return (status);
}

uint8_t SX127xsetCodingRate(CodingRate cr) {
  uint8_t state = SX127xconfig(_bw, _sf, cr, _freq, _syncWord);
  if (state == ERR_NONE) {
    _cr = cr;
  }
  return (state);
}

uint8_t SX127xsetFrequency(float freq) {

  uint8_t status = ERR_NONE;

  status = SX127xsetMode(SX127X_SLEEP);
  if (status != ERR_NONE) {
    return (status);
  }

#define FREQ_STEP                                   61.03515625

  uint32_t FRQ = ( uint32_t )( ( double )freq / ( double )FREQ_STEP );
  status = setRegValue( SX127X_REG_FRF_MSB, ( uint8_t )( ( FRQ >> 16 ) & 0xFF ) );
  status = setRegValue( SX127X_REG_FRF_MID, ( uint8_t )( ( FRQ >> 8 ) & 0xFF ) );
  status = setRegValue( SX127X_REG_FRF_LSB, ( uint8_t )( FRQ & 0xFF ) );

  // set carrier frequency  CHANGED
  // uint32_t base = 2;
  // uint32_t FRF = (freq * (base << 18)) / 32.0;
  // status = setRegValue(SX127X_REG_FRF_MSB, (FRF & 0xFF0000) >> 16);
  // status = setRegValue(SX127X_REG_FRF_MID, (FRF & 0x00FF00) >> 8);
  // status = setRegValue(SX127X_REG_FRF_LSB, FRF & 0x0000FF);

  if (status != ERR_NONE) {
    return (status);
  }

  status = SX127xsetMode(SX127X_STANDBY);
  return (status);

}



//uint8_t SX127xsetPower() {
//
//  //SX1276Read( REG_LR_PACONFIG, &SX1276LR->RegPaConfig );
//  //SX1276Read( REG_LR_PADAC, &SX1276LR->RegPaDac );
//
//  //read SX127X_REG_PA_CONFIG
//  //read SX1278_REG_PA_DAC
//
//  if ( ( SX1276LR->RegPaConfig & RFLR_PACONFIG_PASELECT_PABOOST ) == RFLR_PACONFIG_PASELECT_PABOOST )
//  {
//    if ( ( SX1276LR->RegPaDac & 0x87 ) == 0x87 )
//    {
//      if ( power < 5 )
//      {
//        power = 5;
//      }
//      if ( power > 20 )
//      {
//        power = 20;
//      }
//      SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_MAX_POWER_MASK ) | 0x70;
//      SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 5 ) & 0x0F );
//    }
//    else
//    {
//      if ( power < 2 )
//      {
//        power = 2;
//      }
//      if ( power > 17 )
//      {
//        power = 17;
//      }
//      SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_MAX_POWER_MASK ) | 0x70;
//      SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power - 2 ) & 0x0F );
//    }
//  }
//  else
//  {
//    if ( power < -1 )
//    {
//      power = -1;
//    }
//    if ( power > 14 )
//    {
//      power = 14;
//    }
//    SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_MAX_POWER_MASK ) | 0x70;
//    SX1276LR->RegPaConfig = ( SX1276LR->RegPaConfig & RFLR_PACONFIG_OUTPUTPOWER_MASK ) | ( uint8_t )( ( uint16_t )( power + 1 ) & 0x0F );
//  }
//  SX1276Write( REG_LR_PACONFIG, SX1276LR->RegPaConfig );
//  LoRaSettings.Power = power;
//}
//
//}


uint8_t SX127xsetContTX() {

}

void GetRFmoduleID() {

}


uint8_t SX127xBegin() {
  uint8_t i = 0;
  bool flagFound = false;
  while ((i < 10) && !flagFound) {
    uint8_t version = readRegister(SX127X_REG_VERSION);
    if (version == 0x12) {
      flagFound = true;
    } else {
#ifdef DEBUG
      Serial.print(SX127xgetChipName());
      Serial.print(" not found! (");
      Serial.print(i + 1);
      Serial.print(" of 10 tries) REG_VERSION == ");

      char buffHex[5];
      sprintf(buffHex, "0x%02X", version);
      Serial.print(buffHex);
      Serial.println();
#endif
      delay(1000);
      i++;
    }
  }

  if (!flagFound) {
#ifdef DEBUG
    Serial.print(SX127xgetChipName());
    Serial.println(" not found!");
#endif
    SPI.end();
    return (ERR_CHIP_NOT_FOUND);
  }
#ifdef DEBUG
  else {
    Serial.print(SX127xgetChipName());
    Serial.println(" found! (match by REG_VERSION == 0x12)");
  }
#endif
  return (ERR_NONE);
}

uint8_t SX127xTX(char* data, uint8_t length) {
  SX127xsetMode(SX127X_STANDBY);

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_TX_DONE, 7, 6);

  SX127xclearIRQFlags();

  if (length >= 256) {
    return (ERR_PACKET_TOO_LONG);
  }

  setRegValue(SX127X_REG_PAYLOAD_LENGTH, length);
  setRegValue(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);

  writeRegisterBurstStr(SX127X_REG_FIFO, data, length);

#if defined(ARDUINO_ESP32_DEV)
  digitalWrite(RXenablePin, LOW);
  digitalWrite(TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled
#endif

  SX127xsetMode(SX127X_TX);

  unsigned long start = millis();
  while (!digitalRead(dio0)) {
    delay(1);
    //TODO: calculate timeout dynamically based on modem settings
    if (millis() - start > (length * 1500)) {
      SX127xclearIRQFlags();
      return (ERR_TX_TIMEOUT);
    }
  }

  SX127xclearIRQFlags();

  return (ERR_NONE);

#if defined(ARDUINO_ESP32_DEV)
  digitalWrite(RXenablePin, LOW);
  digitalWrite(TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
#endif

}


//////////////////////////////////////////////////////////////////////////////////////////
void IRAM_ATTR timer_isr_handler()
{
  static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  //Serial.println("TIMERISR");

  //TIMERG0.hw_timer[timer_idx].update = 1;
  // any other code required to reset the timer for next timeout event goes here

  xSemaphoreGiveFromISR(timer_sem, &xHigherPriorityTaskWoken);
  if ( xHigherPriorityTaskWoken) {
    portYIELD_FROM_ISR(); // this wakes up sample_timer_task immediately
  }
}


void IRAM_ATTR sample_timer_task(void *param)
{
  timer_sem = xSemaphoreCreateBinary();

  // timer group init and config goes here (timer_group example gives code for doing this)
  //timer_isr_register(timer_group, timer_idx, timer_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);

  timer = timerBegin(0, 40, true);
  timerAttachInterrupt(timer, &timer_isr_handler, true);
  timerAlarmWrite(timer, 12000, true);
  timerAlarmEnable(timer);


  while (1) {
    xSemaphoreTake(timer_sem, portMAX_DELAY);
    TXstartMicros = micros();

    FHSScounter = FHSScounter + 1;
    if (FHSScounter > 125) {
      FHSScounter = 0;
      SX127xsetFrequency(getNextFreq915());
    }



    // sample sensors via i2c here
    SX127xTXnb(TestData, 6);
    digitalWrite(LEDpin, pinstate);
    pinstate = !pinstate;


    // push sensor data to another queue, or send to a socket...
    //Serial.println("TASK1");

  }
}

/////////////////////////////////////////////////////////////////////////////////////////

void StartTX() {
  xTaskCreate(
    sample_timer_task,          /* Task function. */
    "sample_timer_task",        /* String with name of task. */
    10000,            /* Stack size in words. */
    NULL,             /* Parameter passed as input of the task */
    10,                /* Priority of the task. */
    NULL);            /* Task handle. */
}

/////////////////////////////////////TX functions/////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(ARDUINO_ESP8266_ESP01) //kuldges to fix the shit and manually poll becuase I'm retarded, to be removed in next hardware revision

void ICACHE_RAM_ATTR timer0_ISRtx(void) {

  if (digitalRead(dio0)) {
    SX127xTXnbISR();
    timer0_detachInterrupt();
  } else {
    timer0_write(ESP.getCycleCount() + 16000);
  }
}

void ICACHE_RAM_ATTR timer0_ISRrx(void) {

  if (digitalRead(dio0)) {
    SX127xRXnbISR();
    //timer0_detachInterrupt();
  } else {
    timer0_write(ESP.getCycleCount() + 16000);
  }
}

#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR SX127xTXnbISR() {

#if defined(ARDUINO_ESP32_DEV)
  digitalWrite(TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
  //detachInterrupt(dio0);
#endif

  //LastTXdoneMicros = micros();
  //Serial.println(TXstartMicros - LastTXdoneMicros);
  //CalcOnAirTime();
  //SX127xclearIRQFlags();
  //writeRegisterNR(SX127X_REG_IRQ_FLAGS, 0b11111111);

  LastTXdoneMicros = micros();
  //Serial.println(LastTXdoneMicros - TXstartMicros);

}

uint8_t IRAM_ATTR SX127xTXnbShortVersion(char* data, uint8_t length) {

  SX127xsetMode(SX127X_STANDBY);

  setRegValue(SX127X_REG_PAYLOAD_LENGTH, length);
  setRegValue(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);

  writeRegisterBurstStr(SX127X_REG_FIFO, data, length);

#if defined(ARDUINO_ESP32_DEV)

  digitalWrite(RXenablePin, LOW);
  digitalWrite(TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled
  //attachInterrupt(digitalPinToInterrupt(dio0), SX127xTXnbISR, RISING);

#endif

  SX127xsetMode(SX127X_TX);

  return (ERR_NONE);

}


uint8_t IRAM_ATTR SX127xTXnb(char* data, uint8_t length) {


  SX127xclearIRQFlags();

  SX127xsetMode(SX127X_STANDBY);

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_TX_DONE, 7, 6);
  SX127xclearIRQFlags();

  if (length >= 256) {
    return (ERR_PACKET_TOO_LONG);
  }

  setRegValue(SX127X_REG_PAYLOAD_LENGTH, length);
  setRegValue(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);

  writeRegisterBurstStr(SX127X_REG_FIFO, data, length);

#if defined(ARDUINO_ESP32_DEV)

  digitalWrite(RXenablePin, LOW);
  digitalWrite(TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled
  //attachInterrupt(digitalPinToInterrupt(dio0), SX127xTXnbISR, RISING);

#else

  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(timer0_ISRtx);
  timer0_write(ESP.getCycleCount() + 10000);
  interrupts();

#endif

  SX127xsetMode(SX127X_TX);

  return (ERR_NONE);
}

///////////////////////////////////RX Functions Non-Blocking///////////////////////////////////////////
//
//#if defined(ARDUINO_ESP32_DEV)
//void SX127xRXnbISR() {
//#endif
//
//#if defined(ARDUINO_ESP8266_ESP01)
//  void SX127xRXnbISR() {
//#endif

void SX127xRXnbISR() {
  //Serial.println("rxISRprocess");

  //    if(getRegValue(SX127X_REG_IRQ_FLAGS, 5, 5) == SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR) {
  //    Serial.println("CRC MISMTACH");
  //        return(ERR_CRC_MISMATCH);
  //    }
  //
  if (headerExplMode) {
    RXbuffLen = getRegValue(SX127X_REG_RX_NB_BYTES);
  }

  readRegisterBurstStr(SX127X_REG_FIFO, RXbuffLen, RXbuff);
  //CalcCRC16();

  SX127xclearIRQFlags();
  //Serial.println(".");
  RFbufftoChannels();
  digitalWrite(LEDpin, ledvalue);
  ledvalue = !ledvalue;
}

uint8_t SX127xRXnb() {  //ADDED CHANGED

  // attach interrupt to DIO0
  //RX continuous mode

  // read packet details

#if defined(ARDUINO_ESP32_DEV)
  digitalWrite(TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
  digitalWrite(RXenablePin, HIGH);
#endif

  SX127xsetMode(SX127X_STANDBY);

  SX127xclearIRQFlags();

  if (headerExplMode = false) {

    setRegValue(SX127X_REG_PAYLOAD_LENGTH, RXbuffLen);
  }

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RX_DONE | SX127X_DIO1_RX_TIMEOUT, 7, 4);

  setRegValue(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);

  SX127xsetMode(SX127X_RXCONTINUOUS);

#if defined(ARDUINO_ESP32_DEV)
  attachInterrupt(digitalPinToInterrupt(dio0), SX127xRXnbISR, RISING);

#else

  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(timer0_ISRrx);
  timer0_write(ESP.getCycleCount() + 10000);
  interrupts();

#endif

  return (ERR_NONE);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t SX127xrxSingle(char* data, uint8_t* length, bool headerExplMode) {
  SX127xsetMode(SX127X_STANDBY);

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RX_DONE | SX127X_DIO1_RX_TIMEOUT, 7, 4);
  clearIRQFlags();

  setRegValue(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);

  SX127xsetMode(SX127X_RXSINGLE);

  while (!digitalRead(_dio0)) {
    if (digitalRead(_dio1)) {
      clearIRQFlags();
      return (ERR_RX_TIMEOUT);
    }
  }

  if (getRegValue(SX127X_REG_IRQ_FLAGS, 5, 5) == SX127X_CLEAR_IRQ_FLAG_PAYLOAD_CRC_ERROR) {
    return (ERR_CRC_MISMATCH);
  }

  if (headerExplMode) {
    *length = getRegValue(SX127X_REG_RX_NB_BYTES);
  }

  readRegisterBurstStr(SX127X_REG_FIFO, *length, data);

  clearIRQFlags();

  return (ERR_NONE);
}

uint8_t SX127xrunCAD() {
  SX127xsetMode(SX127X_STANDBY);

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_CAD_DONE | SX127X_DIO1_CAD_DETECTED, 7, 4);
  clearIRQFlags();

  SX127xsetMode(SX127X_CAD);

  while (!digitalRead(_dio0)) {
    if (digitalRead(_dio1)) {
      clearIRQFlags();
      return (PREAMBLE_DETECTED);
    }
  }

  clearIRQFlags();
  return (CHANNEL_FREE);
}

uint8_t IRAM_ATTR SX127xsetMode(uint8_t mode) { //if radio is not already in the required mode set it to the requested mode

  //if (!(_opmode == mode)) {
  setRegValue(SX127X_REG_OP_MODE, mode, 2, 0);
  // _opmode = (RadioOPmodes)mode;
  return (ERR_NONE);
  //  } else {
  //    if (DebugVerbosity >= DEBUG_3) {
  //      Serial.print("OPMODE was already at requested value: ");
  //      printOPMODE(mode);
  //      Serial.println();
  //    }
  //  }
}

uint8_t SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, float freq, uint8_t syncWord) {

  uint8_t status = ERR_NONE;

  // set mode to SLEEP
  status = SX127xsetMode(SX127X_SLEEP);
  if (status != ERR_NONE) {
    return (status);
  }

  // set LoRa mode
  status = setRegValue(SX127X_REG_OP_MODE, SX127X_LORA, 7, 7);
  if (status != ERR_NONE) {
    return (status);
  }

  SX127xsetFrequency(freq);


  // output power configuration
  status = setRegValue(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_OUTPUT_POWER);
  status = setRegValue(SX127X_REG_OCP, SX127X_OCP_ON | SX127X_OCP_TRIM, 5, 0);
  status = setRegValue(SX127X_REG_LNA, SX127X_LNA_GAIN_1 | SX127X_LNA_BOOST_ON);
  if (status != ERR_NONE) {
    return (status);
  }

  // turn off frequency hopping
  status = setRegValue(SX127X_REG_HOP_PERIOD, SX127X_HOP_PERIOD_OFF);
  if (status != ERR_NONE) {
    return (status);
  }

  // basic setting (bw, cr, sf, header mode and CRC)
  if (sf == SX127X_SF_6) {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
  } else {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
  }

  if (status != ERR_NONE) {
    return (status);
  }

  // set the sync word
  status = setRegValue(SX127X_REG_SYNC_WORD, syncWord);
  if (status != ERR_NONE) {
    return (status);
  }

  // set default preamble length
  status = setRegValue(SX127X_REG_PREAMBLE_MSB, SX127X_PREAMBLE_LENGTH_MSB);
  status = setRegValue(SX127X_REG_PREAMBLE_LSB, SX127X_PREAMBLE_LENGTH_LSB);
  if (status != ERR_NONE) {
    return (status);
  }

  // set mode to STANDBY
  status = SX127xsetMode(SX127X_STANDBY);
  return (status);
}


int8_t IRAM_ATTR SX127xgetLastPacketRSSI() {
  return (-157 + getRegValue(SX127X_REG_PKT_RSSI_VALUE));
}

float IRAM_ATTR SX127xgetLastPacketSNR() {
  int8_t rawSNR = (int8_t)getRegValue(SX127X_REG_PKT_SNR_VALUE);
  return (rawSNR / 4.0);
}

void IRAM_ATTR SX127xclearIRQFlags() {
  writeRegister(SX127X_REG_IRQ_FLAGS, 0b11111111);
}

const char* SX127xgetChipName() {
  const char* names[] = {"SX1272", "SX1273", "SX1276", "SX1277", "SX1278", "SX1279"};
  //return(names[_ch]);
  return 0;
}

void IRAM_ATTR CalcOnAirTime() {
  //Serial.println(LastTXdoneMicros - TXstartMicros);
  TimeOnAir = LastTXdoneMicros - TXstartMicros;
}


void IRAM_ATTR GetPacketRSSI_SNR() {
  LastPacketRSSI = SX127xgetLastPacketRSSI();
  LastPacketSNR = SX127xgetLastPacketSNR();

}


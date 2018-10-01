#include "SX127x.h"
#define DEBUG

char TXbuff[256];
char RXbuff[256];



byte RXbuffLen = 6;
byte TXbuffLen = 6;
extern bool headerExplMode = false;

uint32_t TXstartMicros;
uint32_t LastTXdoneMicros;
extern bool radioIdle = true;


///////////Packet Statistics//////////
int8_t LastPacketRSSI = 0;
float LastPacketSNR = 0;
float PacketLossRate = 0;
////////////////////////////////////


void GetRFmoduleID(){
  
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

  SX127xsetMode(SX127X_TX);

  unsigned long start = millis();
  while (!digitalRead(dio0)) {
    //TODO: calculate timeout dynamically based on modem settings
    if (millis() - start > (length * 1500)) {
      SX127xclearIRQFlags();
      return (ERR_TX_TIMEOUT);
    }
  }

  SX127xclearIRQFlags();

  return (ERR_NONE);
}


//void printBuff() {
//  for (int i = 0; i < RXbuffLen; i++) {
//    Serial.print(RXbuff[i], HEX);
//  }
//  Serial.println("");
//}


/////////////////////////////////////TX functions/////////////////////////////////////////

void SX127xTXnbISR() {
  detachInterrupt(dio0);

  LastTXdoneMicros = micros();
  //Serial.println(LastTXdoneMicros - TXstartMicros);
  //Serial.println("txISRprocess");

  SX127xclearIRQFlags();
  radioIdle = true;

}

uint8_t SX127xTXnb(char* data, uint8_t length) {

  TXstartMicros = micros();

  if (!radioIdle) {
    return 99;
  }

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

  SX127xsetMode(SX127X_TX);

  attachInterrupt(digitalPinToInterrupt(dio0), SX127xTXnbISR, RISING);
  return (ERR_NONE);
}

///////////////////////////////////RX Functions Non-Blocking///////////////////////////////////////////



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

  SX127xclearIRQFlags();
  // printBuff();
  RFbufftoChannels();
}

uint8_t SX127xRXnb() {  //ADDED CHANGED

  // attach interrupt to DIO0
  //RX continuous mode

  // read packet details

  SX127xsetMode(SX127X_STANDBY);

  SX127xclearIRQFlags();

  if (headerExplMode = false) {

    setRegValue(SX127X_REG_PAYLOAD_LENGTH, RXbuffLen);
  }

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RX_DONE | SX127X_DIO1_RX_TIMEOUT, 7, 4);

  setRegValue(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);

  SX127xsetMode(SX127X_RXCONTINUOUS);

  attachInterrupt(digitalPinToInterrupt(dio0), SX127xRXnbISR, RISING);

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

uint8_t SX127xsetMode(uint8_t mode) {
  setRegValue(SX127X_REG_OP_MODE, mode, 2, 0);
  return (ERR_NONE);
}

uint8_t SX127xconfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord) {
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
#define FREQ_STEP                                   61.03515625
  uint32_t FRQ = ( uint32_t )( ( double )freq / ( double )FREQ_STEP );
  setRegValue( SX127X_REG_FRF_MSB, ( uint8_t )( ( FRQ >> 16 ) & 0xFF ) );
  setRegValue( SX127X_REG_FRF_MID, ( uint8_t )( ( FRQ >> 8 ) & 0xFF ) );
  setRegValue( SX127X_REG_FRF_LSB, ( uint8_t )( FRQ & 0xFF ) );

  // set carrier frequency  CHANGED
  // uint32_t base = 2;
  // uint32_t FRF = (freq * (base << 18)) / 32.0;
  // status = setRegValue(SX127X_REG_FRF_MSB, (FRF & 0xFF0000) >> 16);
  // status = setRegValue(SX127X_REG_FRF_MID, (FRF & 0x00FF00) >> 8);
  // status = setRegValue(SX127X_REG_FRF_LSB, FRF & 0x0000FF);

  if (status != ERR_NONE) {
    return (status);
  }

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

int8_t SX127xgetLastPacketRSSI() {
  return (-157 + getRegValue(SX127X_REG_PKT_RSSI_VALUE));
}

float SX127xgetLastPacketSNR() {
  int8_t rawSNR = (int8_t)getRegValue(SX127X_REG_PKT_SNR_VALUE);
  return (rawSNR / 4.0);
}

void SX127xclearIRQFlags() {
  writeRegister(SX127X_REG_IRQ_FLAGS, 0b11111111);
}

const char* SX127xgetChipName() {
  const char* names[] = {"SX1272", "SX1273", "SX1276", "SX1277", "SX1278", "SX1279"};
  //return(names[_ch]);
  return 0;
}


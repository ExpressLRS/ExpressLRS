void printBits(byte b)
{
  for (int i = 7; i >= 0; i--)
    Serial.print(bitRead(b, i));
}


void printOPMODE(uint8_t opmode) {
  switch (opmode) {
//    case 0b00000000:  // this too disabled for now
//      Serial.print("SX127X_FSK_OOK");
    case 0b10000000:
      Serial.print("SX127X_LORA");
      break;
//    case 0b00000000: //disabled these two for now until I work out if/when they are needed
//      Serial.print("SX127X_ACCESS_SHARED_REG_OFF");
//    case 0b01000000:
//      Serial.print("SX127X_ACCESS_SHARED_REG_ON");
    case 0b00000000:
      Serial.print("SX127X_SLEEP");
      break;
    case 0b00000001:
      Serial.print("SX127X_STANDBY");
      break;
    case 0b00000010:
      Serial.print("SX127X_FSTX");
      break;
    case 0b00000011:
      Serial.print("SX127X_TX");
      break;
    case 0b00000100:
      Serial.print("SX127X_FSRX");
      break;
    case 0b00000101:
      Serial.print("SX127X_RXCONTINUOUS");
      break;
    case 0b00000110:
      Serial.print("SX127X_RXSINGLE");
      break;
    case 0b00000111:
      Serial.print("SX127X_CAD");
      break;
    default:
      break;
  }
}


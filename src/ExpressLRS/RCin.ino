//Functions related to processing of RC input data

extern SerialRXmode SerRXmode;
extern TrainerMode TrainTXmode;
extern SerialTXmode SerTXmode;
extern SerialTelmMode SerTelmMode;

void ProcessRC() {
  ProcessRCin();
  ProcessRCout();
  ProccessTLM();

}

void ProccessTLM() {
  switch (SerTelmMode) {
    case PROTO_RX_SBUS:
      break;
    case PROTO_RX_PXX:
      break;
    case PROTO_RX_CRSF:
      break;
    case PROTO_RX_NONE:
      break;
    default:
      return;
  }
}

void ProcessRCout() {
  switch (SerTXmode) {
    case PROTO_RX_SBUS:
      break;
    case PROTO_RX_PXX:
      break;
    case PROTO_RX_CRSF:
      break;
    case PROTO_RX_NONE:
      break;
    default:
      return;
  }
}

void ProcessRCin() {

  switch (SerRXmode) {
    case PROTO_RX_SBUS:
      SBUSprocess();
    case PROTO_RX_PXX:
      break;
    case PROTO_RX_CRSF:
      break;
    case PROTO_RX_NONE:
      break;
    default:
      return;
  }
}

void InitRC() {
  switch (SerRXmode) {
    case PROTO_RX_SBUS:
      beginSBUS();
    case PROTO_RX_PXX:
      break;
    case PROTO_RX_CRSF:
      break;
    case PROTO_RX_NONE:
      break;
    default:
      return;
  }
}

void RFbufftoChannels() {  //recombine values into 10 bits from RF data

  ChannelData[0] = RXbuff[0] + ((RXbuff[4] & 0b11000000) << 2);
  ChannelData[1] = RXbuff[1] + ((RXbuff[4] & 0b00110000) << 4);
  ChannelData[2] = RXbuff[2] + ((RXbuff[4] & 0b00001100) << 6);
  ChannelData[3] = RXbuff[3] + ((RXbuff[4] & 0b00000011) << 8);
  ChannelData[4] = ((RXbuff[5] & 0b11000000) << 3);
  ChannelData[5] = ((RXbuff[5] & 0b00110000) << 5);
  ChannelData[6] = ((RXbuff[5] & 0b00001100) << 7);
  ChannelData[7] = ((RXbuff[5] & 0b00000011) << 9);

}


//Functions related to processing of RC input data

enum BaudRates {SER_2400, SER_4800, SER_9600, SER_57600, SER_115200};
enum SerialRXmode {PROTO_RX_NONE, PROTO_RX_SBUS, PROTO_RX_PXX, PROTO_RX_CRSF};
enum TrainerMode {TRN_None, PROTO_TRN_PPM, PROTO_TRN_SBUS};
enum SerialTXmode {TX_None, PROTO_TX_SBUS, PROTO_TX_PXX, PROTO_TX_CRSF};
enum SerialTelmMode {TLM_None, PROTO_TLM_SPORT, PROTO_TLM_CRSF};


SerialRXmode SerRXmode = PROTO_RX_SBUS;
TrainerMode TrainTXmode = PROTO_TRN_SBUS;
SerialTXmode SerTXmode = PROTO_TX_SBUS;
SerialTelmMode SerTelmMode = PROTO_TLM_SPORT;




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


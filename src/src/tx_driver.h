#pragma once

#include "targets.h"
#include "common.h"

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
#include "button.h"
extern button button;
#endif

void TxInitSerial(HardwareSerial& port, uint32_t baudRate);
void TxInitLeds();
void TxInitBuzzer();

void TxLEDShowRate(expresslrs_RFrates_e rate);
void TxSetLEDGreen(uint8_t value);
void TxSetLEDRed(uint8_t value);

void TxBuzzerPlay(unsigned int freq, unsigned long duration);

void TxHandleRadioInitError();

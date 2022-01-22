#pragma once
#include "targets.h"

void hmacSetup();
void hmacReset();
void hmacTeardown();
void getUIDHash(byte *hash, uint8_t len);
uint16_t getHMAC(byte *src, uint8_t len);


#pragma once

#include <Arduino.h>
#include "hamming.h"

/**
 * @brief The below functions may look flexible, but they are not!
 * Only send it an 8B payload that will go into a 14B FECBuffer.
 * The encoding will be ok but interleaving expects 8B going into 14B.
 */

void FECEncode(uint8_t *incomingData, uint8_t *FECBuffer);
void FECDecode(uint8_t *incomingFECBuffer, uint8_t *outgoingData);

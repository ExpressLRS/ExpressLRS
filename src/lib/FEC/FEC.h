#pragma once

#include "hamming.h"

/**
 * @brief Hamming(7,4) + Interleaving
 * 
 * The below functions may look flexible, but they are not!
 * Only send it an 8B payload that will go into a 14B FECBuffer.
 * The encoding will be ok but interleaving expects 8B going into 14B.
 * 
 * Hamming(7,4) - 4b chucks are converted into a 7b codeword.  Each
 * codeword can correct a single bit error during transmission.
 * 
 * Interleaving - The 7b codeword is spread across the entire packet.
 * A single bit is placed in every second byte of the payload.  This 
 * should provide the best defence against burst interference, and single 
 * bit errors in each codeword can be repaired.
 */

void FECEncode(uint8_t *incomingData, uint8_t *FECBuffer);
void FECDecode(uint8_t *incomingFECBuffer, uint8_t *outgoingData);

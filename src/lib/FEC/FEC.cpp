#include "FEC.h"

void FECEncode(uint8_t *incomingData, uint8_t *FECBuffer)
{
    // Encode Hamming(7,4) 
    uint8_t encodedBuffer[8 * 2] = {0};
    for (uint8_t i = 0; i < 8; i++)
    {
        encodedBuffer[i * 2 + 0] = HammingTableEncode(incomingData[i] & 0x0F);  // LSB nibble
        encodedBuffer[i * 2 + 1] = HammingTableEncode(incomingData[i] >> 4);    // MSB nibble
    }

    // Interleaving
    for (uint8_t i = 0; i < (14 / 2); i++)
    {
        for (uint8_t j = 0; j < 8; j++)
        {
            FECBuffer[i * 2 + 0] |= ((encodedBuffer[j + 0] >> i) & 0x01) << j; 
            FECBuffer[i * 2 + 1] |= ((encodedBuffer[j + 8] >> i) & 0x01) << j; 
        }
    }
}

void FECDecode(uint8_t *incomingFECBuffer, uint8_t *outgoingData)
{
    // Interleaving
    uint8_t encodedBuffer[16] = {0};
    for (uint8_t i = 0; i < 8; i++)
    {
        for (uint8_t j = 0; j < 7; j++)
        {
            encodedBuffer[i + 0] |= ((incomingFECBuffer[j * 2 + 0] >> i) & 0x01) << j; 
            encodedBuffer[i + 8] |= ((incomingFECBuffer[j * 2 + 1] >> i) & 0x01) << j; 
        }
    }

    // Decode Hamming(7,4) 
    for (uint8_t i = 0; i < 8; i++)
    {
        outgoingData[i] =  HammingTableDecode(encodedBuffer[i * 2 + 0]);         // LSB nibble
        outgoingData[i] |= HammingTableDecode(encodedBuffer[i * 2 + 1]) << 4;    // MSB nibble
    }
}

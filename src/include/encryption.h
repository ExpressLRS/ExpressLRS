/**
 * @file encryption.h
 * @author Michael Aguayo
 * @brief This file was added from PrivacyLRS fork that implements encryption for ELRS.
 * @date 2025-05-12
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once

#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include <cstdint>
#include <climits>

#define stringify_literal(x) # x
#define stringify_expanded(x) stringify_literal(x)

/**
 * @brief Encryption data params
 * nonce - The current nonce value used to encrypt a msg.
 * key   - The current session key.
 */
typedef struct encryption_params_s
{
  uint8_t nonce[8];
  uint8_t key[16];
} encryption_params_t;

/**
 * @brief This function encrypts a message using ChaCha.
 * @param[out] output  buffer to output the ecrypted data
 * @param[in] input buffer to be encrypt
 * @param[in] length size of the input buffer in bytes
 * @param[in] fhss pointer to the FHSS index
 * @param[in] otaNonce the otaNonce. Used for crypto nonce.
 * @param[in] crcLow the actual crc byte from the input buffer. Used for crypto nonce
 */
void ICACHE_RAM_ATTR encryptMsg(uint8_t *output, uint8_t *input, size_t length, uint8_t fhss, uint8_t otaNonce, uint8_t crcLow);

/**
 * @brief This function decrypts a message using ChaCha.
 * @param[out] output buffer to output decrypted message.
 * @param[in] input buffer to be decrypted.
 * @param[in] length size of input buffer in bytes.
 * @param[in] fhss the current FHSS index. Used for crypto nonce.
 * @param[in] otaNonce the otaNonce. Used for crypto nonce.
 */
void ICACHE_RAM_ATTR decryptMsg(uint8_t *output, uint8_t *input, size_t length, uint8_t fhss, uint8_t otaNonce);

/**
 * @brief initialize the crypto cipher block.
 * @return true on success, false otherwise
 */
bool initCrypto();

/**
 * @brief
 * @param[in] inBuffer
 */
void setCryptoEnable(uint8_t *inBuffer);

/**
 * @brief This function takes a hex string and converts it to an arr of hex vals.
 * @param[out] out pointer to output array.
 * @param[in] in pointer to input char array.
 * @param[out] out_len_max max output length in bytes.
 * @return number of bytes written to the output buffer.
 */
int hexStr2Arr(uint8_t *out, const char *in, size_t out_len_max = 0);

// #endif /* USE_ENCRYPTION */
#endif /* ENCRYPTION_H */
/**
 * @file sha256.h

 * @brief This file contains the public function declarations to be used for any
 *        Sha256
 * @date 2025-05-15
 *
 * @copyright Copyright Neros Technologies (c) 2025
 *
 */

#pragma once

#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

#define SHA256_BLOCK_SIZE 32  // 256 bits

/**
 * @brief Context struct
 * Holds the state, bit length, and buffer for processing data
 */
typedef struct {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t data[64];
  uint32_t datalen;
} SHA256_CTX;

/**
 * @brief Initializes a SHA-256 context.
 *        Must be called before using sha256_update or sha256_final.
 * @param[in] ctx Pointer to SHA256_CTX to initialize
 */
void sha256_init(SHA256_CTX *ctx);

/**
 * @brief Updates the SHA-256 context with new data. Can be called multiple times
 *        with chunks of the input data.
 * @param[in] ctx  Pointer to SHA256_CTX
 * @param[in] data Pointer to data to hash
 * @param[in] len  Length of the data in bytes
 */
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);

/**
 * @brief Finalizes the SHA-256 hash and produces the digest
 *        Call this after all data has been passed to sha256_update.
 * @param[in] ctx  Pointer to SHA256_CTX
 * @param[out] hash Output buffer (must be at least SHA256_BLOCK_SIZE bytes)
 */
void sha256_final(SHA256_CTX *ctx, uint8_t hash[SHA256_BLOCK_SIZE]);

#endif  // SHA256_H

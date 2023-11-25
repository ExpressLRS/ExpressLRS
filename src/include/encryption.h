#pragma once

#ifdef USE_ENCRYPTION

typedef enum : uint8_t {
	ENCRYPTION_STATE_NONE,
  ENCRYPTION_STATE_PROPOSED,
  ENCRYPTION_STATE_FULL,
	ENCRYPTION_STATE_DISABLED
} encryptionState_e;

typedef struct encryption_params_s
{
    uint8_t nonce[8];
    uint8_t key[16];

} encryption_params_t;

bool ICACHE_RAM_ATTR DecryptMsg(uint8_t *input);
void ICACHE_RAM_ATTR EncryptMsg(uint8_t *input, uint8_t *output);

#endif

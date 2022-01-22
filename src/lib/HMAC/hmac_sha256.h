/*
    hmac_sha256.h
    Originally written by https://github.com/h5p9sl
*/

#ifndef _HMAC_SHA256_H_
#define _HMAC_SHA256_H_

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stddef.h>

size_t  // Returns the number of bytes written to `out`
hmac_sha256(
    // [in]: The key and its length.
    //      Should be at least 32 bytes long for optimal security.
    const void* key,
    const size_t keylen,

    // [in]: The data to hash alongside the key.
    const void* data,
    const size_t datalen,

    // [out]: The output hash.
    //      Should be 32 bytes long. If it's less than 32 bytes,
    //      the resulting hash will be truncated to the specified length.
    void* out,
    const size_t outlen);

void
getHash(const void* data,
        const size_t datalen,
        void* out,
        const size_t outlen);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // _HMAC_SHA256_H_

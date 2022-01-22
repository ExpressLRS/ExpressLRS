#include "hmac.h"
#include "logging.h"

#if defined(PLATFORM_ESP32)
#include "mbedtls/md.h"
#include "hmac_sha256.h"
//#include "tinycrypt/hmac.h"
//TCHmacState_t ctx;
mbedtls_md_context_t ctx;
#else
#include "hmac_sha256.h"
#endif

extern uint8_t UID[6];

/*
 *  Usage:      1) call tc_hmac_set_key to set the HMAC key.
 *
 *              2) call tc_hmac_init to initialize a struct hash_state before
 *              processing the data.
 *
 *              3) call tc_hmac_update to process the next input segment;
 *              tc_hmac_update can be called as many times as needed to process
 *              all of the segments of the input; the order is important.
 *
 *              4) call tc_hmac_final to out put the tag.
 */

void hmacSetup() {
    #if defined(PLATFORM_ESP32)
    const size_t keyLength = sizeof(UID);
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);

    if (mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1)) {
        DBGLN("HMAC init failed");
    }
    if (mbedtls_md_hmac_starts(&ctx, (const unsigned char *) UID, keyLength)) {
        DBGLN("HMAC set key failed");
    }
    // if (!tc_hmac_set_key(ctx,UID,sizeof(UID))) {
    //     DBGLN("HMAC set key failed");
    // }
    // if (!tc_hmac_init(ctx)) {
    //     DBGLN("HMAC init failed");
    // }
    #else

    #endif
}

void hmacReset() {
    #if defined(PLATFORM_ESP32)
    mbedtls_md_free(&ctx);
    #endif
    hmacSetup();
}

void hmacTeardown() {
    #if defined(PLATFORM_ESP32)
    mbedtls_md_free(&ctx);
    #endif
}

uint16_t getHMAC(byte *src, uint8_t len) {
    byte hmacResult[32];
    uint16_t result;

    //delay(300);
    //return 0xDEAD;

    hmacSetup();
    #if defined(PLATFORM_ESP32)

    if (mbedtls_md_hmac_update(&ctx, (const unsigned char *) src, len)) {
        DBGLN("HMAC update failed");
    }

    if (mbedtls_md_hmac_finish(&ctx, hmacResult)) {
        DBGLN("HMAC final failed");
    }

    // if (!tc_hmac_update(ctx,src,len)) {
    //     DBGLN("HMAC update failed");
    // }
    // if (!tc_hmac_final(hmacResult,sizeof(hmacResult),ctx) {
    //     DBGLN("HMAC final failed");
    // }

    #else
    if (hmac_sha256(UID,sizeof(UID),src,len,&hmacResult,sizeof(hmacResult)) != sizeof(hmacResult)) {
        Serial.println("#### softare hmac failed #####");
    }

    #endif

    result =  ((uint16_t)hmacResult[0] << 8) + hmacResult[1];

    // Serial.print("Calculating HMAC on: ");

    // for(int i= 0; i< len; i++){
    //     char str[3];

    //     sprintf(str, "%02x", (int)src[i]);
    //     Serial.print(str);
    // }
    // Serial.println();

    // Serial.print("HMAC: ");
    // Serial.print(result,HEX);
    // Serial.println();

    // if (hmac_sha256(UID,sizeof(UID),src,len,&hmacResult,sizeof(hmacResult)) != sizeof(hmacResult)) {
    //     Serial.println("#### softare hmac failed #####");
    // }

    // result =  ((uint16_t)hmacResult[0] << 8) + hmacResult[1];

    // Serial.print("Software HMAC: ");
    // Serial.print(result,HEX);
    // Serial.println();

    hmacTeardown();
    return result;
}


void getUIDHash(byte *result, uint8_t len) {
    // SHA256_HASH digest;

    // //Sha256Calculate(UID,len,&digest);
    // Sha256Context context;

    // Sha256Initialise(&context);
    // Sha256Update(&context, result, len);
    // Sha256Finalise(&context, &digest);

    byte hash[32];

    getHash(UID,sizeof(UID),hash,sizeof(hash));

    // result[0] = digest.bytes[0];
    // result[1] = digest.bytes[1];
    // result[2] = digest.bytes[2];

    result[0] = hash[0];
    result[1] = hash[1];
    result[2] = hash[2];

}

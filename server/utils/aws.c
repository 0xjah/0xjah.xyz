#include "aws.h"

#include <openssl/hmac.h>
#include <openssl/sha.h>

#include <stdio.h>
#include <string.h>

static void hmac_sha256(const unsigned char *key, int keylen,
                        const unsigned char *data, int datalen,
                        unsigned char *out) {
    unsigned int len;
    HMAC(EVP_sha256(), key, keylen, data, datalen, out, &len);
}

static void sha256_hex(const char *data, char *out) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)data, strlen(data), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(out + (i * 2), "%02x", hash[i]);
    }
    out[64] = '\0';
}

void aws_signature_create(const char *method, const char *uri, const char *query,
                          const char *date, const char *datetime, const char *host,
                          const char *access_key, const char *secret_key,
                          char *auth_header, size_t auth_header_size) {
    char canonical_request[4096];
    snprintf(canonical_request, sizeof(canonical_request),
             "%s\n%s\n%s\n"
             "host:%s\n"
             "x-amz-content-sha256:UNSIGNED-PAYLOAD\n"
             "x-amz-date:%s\n\n"
             "host;x-amz-content-sha256;x-amz-date\n"
             "UNSIGNED-PAYLOAD",
             method, uri, query ? query : "", host, datetime);

    char canonical_hash[65];
    sha256_hex(canonical_request, canonical_hash);

    char string_to_sign[1024];
    snprintf(string_to_sign, sizeof(string_to_sign),
             "AWS4-HMAC-SHA256\n%s\n%s/auto/s3/aws4_request\n%s",
             datetime, date, canonical_hash);

    char key[512];
    snprintf(key, sizeof(key), "AWS4%s", secret_key);

    unsigned char k_date[32], k_region[32], k_service[32], k_signing[32];
    hmac_sha256((const unsigned char *)key, (int)strlen(key),
                (const unsigned char *)date, (int)strlen(date), k_date);
    hmac_sha256(k_date, 32, (const unsigned char *)"auto", 4, k_region);
    hmac_sha256(k_region, 32, (const unsigned char *)"s3", 2, k_service);
    hmac_sha256(k_service, 32, (const unsigned char *)"aws4_request", 12, k_signing);

    unsigned char signature[32];
    hmac_sha256(k_signing, 32, (const unsigned char *)string_to_sign,
                (int)strlen(string_to_sign), signature);

    char sig_hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(sig_hex + (i * 2), "%02x", signature[i]);
    }
    sig_hex[64] = '\0';

    snprintf(auth_header, auth_header_size,
             "AWS4-HMAC-SHA256 Credential=%s/%s/auto/s3/aws4_request,"
             "SignedHeaders=host;x-amz-content-sha256;x-amz-date,Signature=%s",
             access_key, date, sig_hex);
}

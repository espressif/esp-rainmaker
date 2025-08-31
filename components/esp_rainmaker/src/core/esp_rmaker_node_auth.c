/*
 * SPDX-FileCopyrightText: 2023-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <esp_log.h>
#include "mbedtls/platform.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ecdsa.h"
#include "sha/sha_parallel_engine.h"

#include "esp_secure_cert_read.h"
#include "esp_rmaker_utils.h"
#include "esp_rmaker_client_data.h"
#include "mbedtls/x509_crt.h"

static const char *TAG = "esp_rmaker_user_node_auth";

static int myrand(void *rng_state, unsigned char *output, size_t len)
{
    esp_fill_random(output, len);
    return 0;
}

static inline uint8_t to_hex_digit(unsigned val)
{
    return (val < 10) ? ('0' + val) : ('a' + val - 10);
}

static void bytes_to_hex(uint8_t *src, uint8_t *dst, int in_len)
{
    for (int i = 0; i < in_len; i++) {
        dst[2 * i] = to_hex_digit(src[i] >> 4);
        dst[2 * i + 1] = to_hex_digit(src[i] & 0xf);
    }
    dst[2 * in_len] = 0;
}

esp_err_t esp_rmaker_node_auth_sign_msg(const void *challenge, size_t inlen, void **response, size_t *outlen)
{
    if (!challenge || (inlen == 0)) {
        ESP_LOGE(TAG, "function arguments challenge and inlen cannot be NULL.");
        return ESP_ERR_INVALID_ARG;
    }

    /* Get private key */
    char *priv_key = NULL;
    size_t priv_key_len = 0;
#if CONFIG_ESP_RMAKER_USE_ESP_SECURE_CERT_MGR
    esp_secure_cert_key_type_t key_type;
    esp_err_t err = esp_secure_cert_get_priv_key_type(&key_type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get the type of private key from secure cert partition, err:%d", err);
        return err;
    }
    if (key_type == ESP_SECURE_CERT_INVALID_KEY) {
        ESP_LOGE(TAG, "Private key type in secure cert partition is invalid");
        return ESP_FAIL;
    }
    /* This flow is for devices supporting ECDSA peripheral */
    if (key_type == ESP_SECURE_CERT_ECDSA_PERIPHERAL_KEY) {
#if CONFIG_USE_ESP32_ECDSA_PERIPHERAL
        /* TODO: code for signing the challenge on devices that have a DS peripheral. */
        return ESP_FAIL;
#else  /* !CONFIG_USE_ESP32_ECDSA_PERIPHERAL */
        return ESP_ERR_INVALID_STATE;
#endif /* CONFIG_USE_ESP32_ECDSA_PERIPHERAL */
    } else
#endif /* CONFIG_ESP_RMAKER_USE_ESP_SECURE_CERT_MGR */
    {
        /* This flow is for devices which do not support ECDSA peripheral */
#if !CONFIG_USE_ESP32_ECDSA_PERIPHERAL
        priv_key = esp_rmaker_get_client_key();
        priv_key_len = esp_rmaker_get_client_key_len();
#else /* CONFIG_USE_ESP32_ECDSA_PERIPHERAL */
        return ESP_ERR_INVALID_STATE;
#endif /* !CONFIG_USE_ESP32_ECDSA_PERIPHERAL */
    }
    if (!priv_key) {
        ESP_LOGE(TAG, "Error getting private key");
        return ESP_FAIL;
    }
    /* Calculate SHA of challenge */
    uint8_t hash[32];
    esp_sha(SHA2_256,(const unsigned char *)challenge, inlen, hash);

    /* Sign the hash using RSA or ECDSA */
    mbedtls_pk_context pk_ctx;
    mbedtls_pk_init(&pk_ctx);
    int ret = mbedtls_pk_parse_key(&pk_ctx, (uint8_t *)priv_key, priv_key_len, NULL, 0, NULL, 0);
    uint8_t *signature = NULL;
    if (mbedtls_pk_get_type(&pk_ctx) == MBEDTLS_PK_RSA) {
        ESP_LOGI(TAG, "RSA key found");
        signature = (uint8_t *)MEM_CALLOC_EXTRAM(1, 256); // TODO: replace magic number 256
    } else if (mbedtls_pk_get_type(&pk_ctx) == MBEDTLS_PK_ECKEY) {
        ESP_LOGI(TAG, "ECDSA key found");
        signature = (uint8_t *)MEM_CALLOC_EXTRAM(1, MBEDTLS_ECDSA_MAX_LEN);
    } else {
        ESP_LOGE(TAG, "found different key: %d", mbedtls_pk_get_type(&pk_ctx));
    }
    if (!signature) {
        ESP_LOGE(TAG, "Failed to allocate memory to signature.");
        mbedtls_pk_free(&pk_ctx);
        return ESP_ERR_NO_MEM;
    }
    size_t slen = 0;
    if (mbedtls_pk_get_type(&pk_ctx) == MBEDTLS_PK_ECKEY) {
        ret = mbedtls_ecdsa_write_signature(mbedtls_pk_ec(pk_ctx), MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, MBEDTLS_ECDSA_MAX_LEN, &slen, myrand, NULL);
        if (ret != 0) {
            ESP_LOGE(TAG, "Error in writing signature. err = %d", ret);
            free(signature);
            mbedtls_pk_free(&pk_ctx);
            return ESP_FAIL;
        }
    } else if (mbedtls_pk_get_type(&pk_ctx) == MBEDTLS_PK_RSA) {
        mbedtls_rsa_context *rsa_ctx = mbedtls_pk_rsa(pk_ctx);
        // rsa_ctx->MBEDTLS_PRIVATE(len) = 256;
        mbedtls_rsa_set_padding(rsa_ctx, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
        ret = mbedtls_rsa_rsassa_pss_sign(rsa_ctx,
                                          myrand,
                                          NULL,
                                          MBEDTLS_MD_SHA256,
                                          sizeof(hash),
                                          hash,
                                          signature);
        if (ret != 0) {
            ESP_LOGE(TAG, "Error in writing signature. err = %d", ret);
            free(signature);
            mbedtls_pk_free(&pk_ctx);
            return ESP_FAIL;
        }
        slen = mbedtls_rsa_get_len(rsa_ctx);
        ESP_LOGI(TAG, "signature length %d", slen);
    }

#if TEST_SIGNATURE_VERIFICATION
    char *cert = esp_rmaker_get_client_cert();
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    mbedtls_x509_crt_parse(&crt, (const unsigned char *)cert, strlen(cert) + 1);
    mbedtls_pk_context *pk = &crt.pk;

    if (mbedtls_pk_get_type(&pk_ctx) == MBEDTLS_PK_ECKEY) {
        ret = mbedtls_pk_verify(pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, sizeof(signature));
    } else if (mbedtls_pk_get_type(&pk_ctx) == MBEDTLS_PK_RSA) {
        mbedtls_pk_rsassa_pss_options opt = {
            .mgf1_hash_id = MBEDTLS_MD_SHA256,
            .expected_salt_len = MBEDTLS_RSA_SALT_LEN_ANY
        };
        ret = mbedtls_pk_verify_ext(MBEDTLS_PK_RSASSA_PSS, (const void *)&opt, pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), signature, slen);
    }

    mbedtls_x509_crt_free(&crt);
    free(cert);
    if (ret == 0) {
        ESP_LOGI(TAG, "Signature is valid");
    } else {
        ESP_LOGE(TAG, "Signature verification failed %d", ret);
    }
#endif

    /* Convert hex stream to bytes */
#define BYTE_ENCODED_SIGNATURE_LEN ((2 * slen) + 1) /* +1 for null character */
    char *char_signature = (char *)MEM_ALLOC_EXTRAM(BYTE_ENCODED_SIGNATURE_LEN);
    if (!char_signature) {
        ESP_LOGE(TAG, "Error in allocating memory for challenge response.");
        free(signature);
        mbedtls_pk_free(&pk_ctx);
        return ESP_ERR_NO_MEM;
    }
    bytes_to_hex(signature, (uint8_t *)char_signature, slen);
    mbedtls_pk_free(&pk_ctx);
    free(signature);
    /* Set output variables */
    *(char **)response = char_signature;
    *outlen = 2 * slen; /* hex encoding takes 2 bytes per input byte */
    return ESP_OK;
}

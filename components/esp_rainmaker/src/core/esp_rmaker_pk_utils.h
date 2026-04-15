/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <esp_idf_version.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include <mbedtls/pk.h>
#include <psa/crypto.h>

/* Detect the type of a parsed software key from its PSA attributes.
 * mbedtls_pk_can_do_psa() is unsuitable here: it returns 0 both for an
 * uninitialised context and when the parsed key's PSA algorithm policy does
 * not match the exact algorithm queried (which a freshly-parsed software key
 * does not satisfy for a specific hash), so it misreports valid EC/RSA keys.
 */
static inline void esp_rmaker_pk_detect_type(const mbedtls_pk_context *pk, int *is_ec, int *is_rsa)
{
    *is_ec = 0;
    *is_rsa = 0;
    psa_key_attributes_t key_attr = PSA_KEY_ATTRIBUTES_INIT;
    if (mbedtls_pk_get_psa_attributes(pk, PSA_KEY_USAGE_SIGN_HASH, &key_attr) == 0) {
        psa_key_type_t key_type = psa_get_key_type(&key_attr);
        *is_ec = PSA_KEY_TYPE_IS_ECC(key_type);
        *is_rsa = PSA_KEY_TYPE_IS_RSA(key_type);
    }
    psa_reset_key_attributes(&key_attr);
}
#endif /* ESP_IDF_VERSION >= 6.0.0 */

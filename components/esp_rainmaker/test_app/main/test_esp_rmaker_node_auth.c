/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Exercises the public esp_rmaker_node_auth_sign_msg() API end-to-end for both
 * supported claim key types (ECDSA P-256 — the default — and RSA-2048 — the
 * fallback). This is the code path the IDF v6.0 (mbedtls 4.0) migration touches
 * in esp_rmaker_node_auth.c: SHA-256 of the challenge, private-key parse,
 * key-type detection and signing (ECDSA or RSA-PSS).
 *
 * Each case provisions a known key + matching cert into the factory store
 * (exactly as claiming does), calls the real API, then verifies the returned
 * signature against the cert's public key. It does NOT re-implement the signing
 * logic, so a regression in the production code makes the test fail.
 */

#include <string.h>
#include <stdbool.h>
#include "unity.h"
#include "esp_log.h"
#include "esp_rmaker_core.h"        /* esp_rmaker_node_auth_sign_msg() */
#include "esp_rmaker_factory.h"     /* esp_rmaker_factory_init/set() */
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "psa/crypto.h"            /* RSA-PSS verify (rsa.h is private in mbedtls 4.0) */

/* NVS keys the client-data layer reads the key/cert from (esp_rmaker_client_data.h). */
#define CLIENT_KEY_NVS_KEY   "client_key"
#define CLIENT_CERT_NVS_KEY  "client_cert"

static const char *TAG = "node_auth_test";

/* EC P-256 test key + matching self-signed cert (CN=rmaker-node-auth-test). */
static const char ec_key[] =
"-----BEGIN EC PRIVATE KEY-----\n"
"MHcCAQEEIMAsrNEjp1e+0Mszy2hTM3YAWU4wTwK63a6tmoutPvExoAoGCCqGSM49\n"
"AwEHoUQDQgAE8qe3JEptAhwoJLd6ob61R+9l7KS8EJp+FNoetmptA8MyOWfbJgAY\n"
"/a7h+6cI9PSj2nX8Z4UTHInOrW8lgngTBw==\n"
"-----END EC PRIVATE KEY-----\n";

static const char ec_cert[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIBljCCATugAwIBAgIUSiQVviJfbAQ/y8pfvN8B2nhdw54wCgYIKoZIzj0EAwIw\n"
"IDEeMBwGA1UEAwwVcm1ha2VyLW5vZGUtYXV0aC10ZXN0MB4XDTI2MDYyOTA5NTA1\n"
"M1oXDTM2MDYyNjA5NTA1M1owIDEeMBwGA1UEAwwVcm1ha2VyLW5vZGUtYXV0aC10\n"
"ZXN0MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE8qe3JEptAhwoJLd6ob61R+9l\n"
"7KS8EJp+FNoetmptA8MyOWfbJgAY/a7h+6cI9PSj2nX8Z4UTHInOrW8lgngTB6NT\n"
"MFEwHQYDVR0OBBYEFCVZ4AL+QjPS+JDTyuMGt61RtdpzMB8GA1UdIwQYMBaAFCVZ\n"
"4AL+QjPS+JDTyuMGt61RtdpzMA8GA1UdEwEB/wQFMAMBAf8wCgYIKoZIzj0EAwID\n"
"SQAwRgIhAOPnJ3vbNtthc3r1QLkGCHHGAf6uIYOLn7VKTthVf5tiAiEA0XZSa4Gx\n"
"VhUtqJegZtYLdx5o/H8OWJbEbcV8QzxCGrc=\n"
"-----END CERTIFICATE-----\n";

/* RSA-2048 test key + matching self-signed cert (CN=rmaker-node-auth-test-rsa). */
static const char rsa_key[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQDh+03Phcs1/FEQ\n"
"kQcbesbbtj0j7Bk6smahaCupR+cAi+lGMTy8A3NVZ9Gqhgsnd/OnMDqohkFJ/XpU\n"
"dLDhIw8akdipc4RJkosZxe2jElHuLOhOcNL4pIlhdBZp3+BO/JpMJUNGm6CsHMvZ\n"
"UCWkdlCzwTpDQcBJkdpIszonYyWOMJs0rB8d5s6XU433N2FEM8nWQhl5LGU/3Zdv\n"
"uMCXhtCJNCVDLQJra+sH6gSLRpMS6HplR0KN+qFNe6+oyH9N7Wwu3Z0F/UBsN0ZY\n"
"6lzvDvjVuCXofALpaBFP3fgCtODhYaY180e+SK3LkMb9RB6sGY/sfS7X9hLeVCi5\n"
"b3X+D9rNAgMBAAECggEAAK0XYQfWO7qCUdgyHHcytR7NKfFlEvkwocTpwspsi3jU\n"
"kIuMTJ/i41MjahEzY1fMy/XJEZRc6EE+rs2o1rGFdwEPfdn0SFRrTv8g3s5icZav\n"
"l20v3MHkwAflUqncNYR3RYvsNnpOHmqVNLBKs39bpkh4l4wkSzsDtHG4UNLlpAsB\n"
"T8VAyVJZJZ6HLT/UuyC72h8DMNyPklzqtOV2TqEiFJ6j36l+YXYNnT1OEacXmwXI\n"
"NiFgWxio3UR6RyhN++5OXOjcLmSF30p/g0foXPfjcfD5SAHU7Gk+ew+eWng7LQ2T\n"
"1XXJteD5Ukuexcj+Bzk5h3t1QLCLf1W4N6Riq6uLQQKBgQD9igOYIwLwtLkcEfxl\n"
"x96Mfy5oaGHWZQINElxwdTl+ro1cAfUmSVszbRsw+DSat4jPbYWOWO2wGijL2eRD\n"
"Rrq6IDNxvEWqaIT7rSCc68tmKbcLVKdharzqqDwsQcyWg9cFdRxpoct7RnzBgziY\n"
"Xe0vIM1T7/w9W4FGS6wq8r0UKQKBgQDkLNDlekKdySTQo6jJiAswwgZJII4NBkSE\n"
"em4KiW7Kg4t2qvET4FXQByj125Knmpy5C7UmIyZsW3Zy/HMI+Yli0ZSmpN3zzWbi\n"
"D+ElnBTUDsrU9FmaXcElWSt44Nbt+2FINgmPpWpf3L1Bg28aZsMQvtGFV5vRWqLh\n"
"VQWjS2mGBQKBgQCMNRB7x5DOsAJWBZRAbOEjbQmq7157g2w48lhwUEM1TS4bHXIG\n"
"dDadH88Tq47bqHddzkE3UPJQmobJBHv4wFpup3QLh4Q3EonH7BvG65aVrqfs+il9\n"
"89r5IYP1LYYIDmyQNg06VSED0R0YnopjqD2L/GXP7zzcaYf56csSDR6G0QKBgQDi\n"
"cPNy+U1JJ4goonJsZl2sqrDwO6WdmV3AF3xDaraPtdJfFzAeqKCgzapBrAGz4YWt\n"
"Qsgoq/OCz5YScSR8gKBueE1MwAsg2/RBXr7ywx4MgQ2cH08ZGfgHZ6Hz2TaTgMi/\n"
"kBkW/HM/5WHQtW46RkdJxAVMjnAtaQSCGQ16S+nSPQKBgGXW4WihRMTZKoEwT6iU\n"
"f/Y2glFUIf8MaN7SoqQSUEKJi2BgFd1lvFbHaFefAUNsf4JQ8CC+SYphmk9FCfw0\n"
"6goTAXbBOsF9Rlj1q8ALd17DTSoSBo7nYxrXewR+rNqTKGQFfuJgWHum5UCDYZwM\n"
"Ee02rkD018lWw57DiW1rPKd0\n"
"-----END PRIVATE KEY-----\n";

static const char rsa_cert[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDKTCCAhGgAwIBAgIUfCvwiEdoggiPA+HyUtP8Kvko0UAwDQYJKoZIhvcNAQEL\n"
"BQAwJDEiMCAGA1UEAwwZcm1ha2VyLW5vZGUtYXV0aC10ZXN0LXJzYTAeFw0yNjA2\n"
"MjkxMDI3MDBaFw0zNjA2MjYxMDI3MDBaMCQxIjAgBgNVBAMMGXJtYWtlci1ub2Rl\n"
"LWF1dGgtdGVzdC1yc2EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDh\n"
"+03Phcs1/FEQkQcbesbbtj0j7Bk6smahaCupR+cAi+lGMTy8A3NVZ9Gqhgsnd/On\n"
"MDqohkFJ/XpUdLDhIw8akdipc4RJkosZxe2jElHuLOhOcNL4pIlhdBZp3+BO/JpM\n"
"JUNGm6CsHMvZUCWkdlCzwTpDQcBJkdpIszonYyWOMJs0rB8d5s6XU433N2FEM8nW\n"
"Qhl5LGU/3ZdvuMCXhtCJNCVDLQJra+sH6gSLRpMS6HplR0KN+qFNe6+oyH9N7Wwu\n"
"3Z0F/UBsN0ZY6lzvDvjVuCXofALpaBFP3fgCtODhYaY180e+SK3LkMb9RB6sGY/s\n"
"fS7X9hLeVCi5b3X+D9rNAgMBAAGjUzBRMB0GA1UdDgQWBBQdkveWPliMReFeSo5l\n"
"qAW3TniWHTAfBgNVHSMEGDAWgBQdkveWPliMReFeSo5lqAW3TniWHTAPBgNVHRMB\n"
"Af8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQDaK5DaVSTr8k4qoKl2EbH3q6oD\n"
"WxdQenf4xPEbxCd88NtcTDSVEJQnW9HZZoJ0eGtwUtf2DW8e9f5Af+hlbFyYEDjs\n"
"gg3MM6IvuW8w45hdbdnL7SBxJkyCRh8GQBSrnhkeft9iur7ULyMhxgTJ9D2jypjb\n"
"e7Xk95bZvedLKXG/lqLZx6zUn0GSATwW3Ja2VmeB9ijml+R00JkjbZVXcAjFfOMp\n"
"VKpsx9Wd5C9eY3zWbX1n1+YqOI2CV8JLwlwb4EKx42zaODCwB2aGkaEskUHqeGfd\n"
"ivm8rsufDg888+2PCKQz8opOu0X7sbja+OgEOlE5y0lEtQqmdLx+CIPK6eLi\n"
"-----END CERTIFICATE-----\n";

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Provision a known key+cert, call the real sign_msg, and verify the returned
 * signature against the cert's public key. */
static void sign_and_verify(const char *key_pem, const char *cert_pem, bool is_rsa)
{
    TEST_ESP_OK(esp_rmaker_factory_init());
    TEST_ESP_OK(esp_rmaker_factory_set(CLIENT_KEY_NVS_KEY, (void *)key_pem, strlen(key_pem)));
    TEST_ESP_OK(esp_rmaker_factory_set(CLIENT_CERT_NVS_KEY, (void *)cert_pem, strlen(cert_pem)));

    const char *challenge = "rmaker-node-auth-challenge";
    char *response = NULL;
    size_t outlen = 0;
    esp_err_t err = esp_rmaker_node_auth_sign_msg(challenge, strlen(challenge), (void **)&response, &outlen);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_NOT_NULL(response);
    TEST_ASSERT_TRUE(outlen > 0 && (outlen % 2) == 0);

    /* Decode the hex signature the API returned. */
    size_t sig_len = outlen / 2;
    uint8_t *sig = malloc(sig_len);
    TEST_ASSERT_NOT_NULL(sig);
    for (size_t i = 0; i < sig_len; i++) {
        int hi = hex_nibble(response[2 * i]);
        int lo = hex_nibble(response[2 * i + 1]);
        TEST_ASSERT_TRUE(hi >= 0 && lo >= 0);
        sig[i] = (uint8_t)((hi << 4) | lo);
    }

    /* Verify against the cert's public key over SHA-256(challenge). */
    uint8_t hash[32];
    TEST_ASSERT_EQUAL(0, mbedtls_md(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                                    (const unsigned char *)challenge, strlen(challenge), hash));

    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    TEST_ASSERT_EQUAL(0, mbedtls_x509_crt_parse(&crt, (const unsigned char *)cert_pem, strlen(cert_pem) + 1));

    if (is_rsa) {
        /* node_auth signs RSA as RSA-PSS. Verify via PSA (portable across mbedtls
         * 3.x/4.0; the mbedtls_pk_verify_ext RSASSA-PSS options type is private in
         * 4.0). PSA_ALG_RSA_PSS_ANY_SALT accepts the signer's salt length. */
        psa_algorithm_t pss = PSA_ALG_RSA_PSS_ANY_SALT(PSA_ALG_SHA_256);
        psa_crypto_init();  /* idempotent; required on pre-v6.0 where PSA isn't auto-init */
        psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
        TEST_ASSERT_EQUAL(0, mbedtls_pk_get_psa_attributes(&crt.pk, PSA_KEY_USAGE_VERIFY_HASH, &attr));
        psa_set_key_algorithm(&attr, pss);
        psa_key_id_t key_id = 0;
        TEST_ASSERT_EQUAL(0, mbedtls_pk_import_into_psa(&crt.pk, &attr, &key_id));
        psa_status_t st = psa_verify_hash(key_id, pss, hash, sizeof(hash), sig, sig_len);
        psa_destroy_key(key_id);
        psa_reset_key_attributes(&attr);
        TEST_ASSERT_EQUAL(PSA_SUCCESS, st);

        /* Enforce the scheme, not just verifiability: the same signature must NOT
         * verify as PKCS#1 v1.5. The cloud accepts RSA-PSS only, so a padding
         * regression to v1.5 (e.g. in the DS-peripheral path) must fail here. */
        psa_algorithm_t v15 = PSA_ALG_RSA_PKCS1V15_SIGN(PSA_ALG_SHA_256);
        psa_key_attributes_t attr_v15 = PSA_KEY_ATTRIBUTES_INIT;
        TEST_ASSERT_EQUAL(0, mbedtls_pk_get_psa_attributes(&crt.pk, PSA_KEY_USAGE_VERIFY_HASH, &attr_v15));
        psa_set_key_algorithm(&attr_v15, v15);
        psa_key_id_t key_id_v15 = 0;
        TEST_ASSERT_EQUAL(0, mbedtls_pk_import_into_psa(&crt.pk, &attr_v15, &key_id_v15));
        st = psa_verify_hash(key_id_v15, v15, hash, sizeof(hash), sig, sig_len);
        psa_destroy_key(key_id_v15);
        psa_reset_key_attributes(&attr_v15);
        TEST_ASSERT_NOT_EQUAL(PSA_SUCCESS, st);
    } else {
        TEST_ASSERT_EQUAL(0, mbedtls_pk_verify(&crt.pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, sig_len));
    }

    mbedtls_x509_crt_free(&crt);
    free(sig);
    free(response);
    ESP_LOGI(TAG, "%s sign_msg signature verified against cert (sig_len=%d)",
             is_rsa ? "RSA-PSS" : "ECDSA", (int)sig_len);
}

TEST_CASE("ESP RainMaker node auth: ECDSA sign_msg verifies against cert", "[rmaker_node_auth]")
{
    sign_and_verify(ec_key, ec_cert, false);
}

TEST_CASE("ESP RainMaker node auth: RSA sign_msg verifies against cert", "[rmaker_node_auth]")
{
    sign_and_verify(rsa_key, rsa_cert, true);
}

TEST_CASE("ESP RainMaker node auth: sign_msg rejects NULL input", "[rmaker_node_auth]")
{
    char *response = NULL;
    size_t outlen = 0;
    esp_err_t err = esp_rmaker_node_auth_sign_msg(NULL, 0, (void **)&response, &outlen);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

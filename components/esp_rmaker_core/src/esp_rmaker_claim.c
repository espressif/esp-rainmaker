// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


#include "mbedtls/config.h"
#include "mbedtls/platform.h"
#include "mbedtls/pk.h"
#include "mbedtls/rsa.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/md.h"
#include "mbedtls/sha512.h"

#include "soc/soc.h"
#include "soc/efuse_reg.h"
#include "esp_efuse.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"


#include <esp_tls.h>
#include <esp_rmaker_core.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_rmaker_storage.h>
#include "esp_rmaker_client_data.h"

#include <json_generator.h>
#include <json_parser.h>

static const char *TAG = "esp_claim";

#define CLAIM_BASE_URL      CONFIG_ESP_RMAKER_CLAIM_BASE_URL
#define CLAIM_INIT_PATH     "initiate"
#define CLAIM_VERIFY_PATH   "verify"

#define CLAIM_PK_SIZE       2048

#define MAX_CSR_SIZE        1024
#define MAX_PAYLOAD_SIZE    3072

typedef struct {
    unsigned char csr[MAX_CSR_SIZE];
    char payload[MAX_PAYLOAD_SIZE];
    mbedtls_pk_context key;
} esp_rmaker_claim_data_t;
esp_rmaker_claim_data_t *g_claim_data;

extern uint8_t claiming_server_root_ca_pem_start[] asm("_binary_claiming_server_crt_start");
extern uint8_t claiming_server_root_ca_pem_end[] asm("_binary_claiming_server_crt_end");

static void escape_new_line(esp_rmaker_claim_data_t *data)
{
    char *str = (char *)data->csr;
    memset(data->payload, 0, sizeof(data->payload));
    char *target_str = (char *)data->payload;
    /* Hack to just avoid a "\r\n" at the end of string */
    if (str[strlen(str) - 1] == '\n') {
        str[strlen(str) - 1] = '\0';
    }
    while (*str) {
        if (*str == '\n') {
            *target_str++ = '\\';
            *target_str++ = 'n';
            str++;
            continue;
        }
        *target_str++ = *str++;
    }
    *target_str = '\0';
    strcpy((char *)data->csr, (char *)data->payload);
    ESP_LOGD(TAG, "Modified CSR : %s", data->csr);
}

static void unescape_new_line(char *str)
{
    char *target_str = str;
    while (*str) {
        if (*str == '\\') {
            str++;
            if (*str == 'n') {
                *target_str++ = '\n';
                str++;
            }
        }
        *target_str++ = *str++;
    }
    *target_str = '\0';
}

static esp_err_t read_hmac_key(uint32_t *out_hmac_key, size_t hmac_key_size)
{
    /* ESP32-S2 HMAC Key programming scheme */
    if (hmac_key_size != 16) {
        ESP_LOGE(TAG, "HMAC key size should be 16 bytes.");
    }
    esp_err_t err = esp_efuse_read_field_blob(ESP_EFUSE_SYS_DATA_PART1, out_hmac_key, hmac_key_size * 8);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_efuse_read_field_blob failed!");
    }
    return err;
}

static esp_err_t hmac_challenge(const char* hmac_request, unsigned char *hmac_response, size_t len_hmac_response)
{
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA512;
    uint32_t hmac_key[4];
    
    esp_err_t err = read_hmac_key(hmac_key, sizeof(hmac_key));
    if (err != ESP_OK) {
        return err;
    }

    mbedtls_md_init(&ctx);  
    int ret = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type) ,1); 
    ret |= mbedtls_md_hmac_starts(&ctx, (const unsigned char *)hmac_key, sizeof(hmac_key));
    ret |= mbedtls_md_hmac_update(&ctx, (const unsigned char *)hmac_request, strlen(hmac_request));    
    ret |= mbedtls_md_hmac_finish(&ctx, hmac_response);   
    mbedtls_md_free(&ctx);

    if(ret == 0) {
        return ESP_OK;
    } else {
        return ret;
    }
}

/* Parse the Claim Init response and generate Claim Verify request
 *
 * Claim Init Response format:
 *  {"claim-id":"<unique-claim-id>", "challenge":"<upto 128 byte challenge>"}
 *
 * Claim Verify Request format
 *  {"claim-id":"<claim-id-from-init>", "challenge-response":"<64byte-response-in-hex>"}
 */
static esp_err_t handle_claim_init_response(esp_rmaker_claim_data_t *claim_data)
{
    ESP_LOGD(TAG, "Claim Init Response: %s", claim_data->payload);
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, claim_data->payload, strlen(claim_data->payload)) == 0) {
        char claim_id[64];
        char challenge[130];
        int ret = json_obj_get_string(&jctx, "claim-id", claim_id, sizeof(claim_id));
        ret |= json_obj_get_string(&jctx, "challenge", challenge, sizeof(challenge));
        json_parse_end(&jctx);
        if (ret == 0) {
            unsigned char response[64] = {0};
            if (hmac_challenge(challenge, response, sizeof(response)) == ESP_OK) {
                json_gen_str_t jstr;
                json_gen_str_start(&jstr, claim_data->payload, sizeof(claim_data->payload), NULL, NULL);
                json_gen_start_object(&jstr);
                json_gen_obj_set_string(&jstr, "claim-id", claim_id);
                /* Add Challenge Response as a hex representation */
                json_gen_obj_start_long_string(&jstr, "challenge-response", NULL);
                for(int i = 0 ; i < sizeof(response); i++) {
                    char hexstr[3];
                    snprintf(hexstr, sizeof(hexstr), "%02X", response[i]);
                    json_gen_add_to_long_string(&jstr, hexstr);
                }
                json_gen_end_long_string(&jstr);
                json_gen_end_object(&jstr);
                json_gen_str_end(&jstr);
                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "HMAC Challenge failed.");
            }
        } else {
            ESP_LOGE(TAG, "Claim Init Response invalid.");
        }
    }
    ESP_LOGE(TAG, "Failed to parse Claim Init Response.");
    return ESP_FAIL;
}
/* Parse the Claim Init response and generate Claim Verify request
 *
 * Claim Verify Response format:
 *  {"certificate":"<certificate>"}
 */
static esp_err_t handle_claim_verify_response(esp_rmaker_claim_data_t *claim_data)
{
    ESP_LOGD(TAG, "Claim Verify Response: %s", claim_data->payload);
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, claim_data->payload, strlen(claim_data->payload)) == 0) {
        int required_len = 0;
        if (json_obj_get_strlen(&jctx, "certificate", &required_len) == 0) {
            required_len++; /* For NULL termination */
            char *certificate =  calloc(1, required_len);
            if (!certificate) {
                json_parse_end(&jctx);
                ESP_LOGE(TAG, "Failed to allocate %d bytes for certificate.", required_len);
                return ESP_ERR_NO_MEM;
            }
            json_obj_get_string(&jctx, "certificate", certificate, required_len);
            json_parse_end(&jctx);
            unescape_new_line(certificate);
            esp_err_t err = esp_rmaker_storage_set(ESP_RMAKER_CLIENT_CERT_NVS_KEY, certificate, strlen(certificate));
            free(certificate);
            return err;
        } else {
            ESP_LOGE(TAG, "Claim Verify Response invalid.");
        }
    }
    ESP_LOGE(TAG, "Failed to parse Claim Verify Response.");
    return ESP_FAIL;
}

static esp_err_t esp_rmaker_claim_perform_common(esp_rmaker_claim_data_t *claim_data, const char *path)
{
    char url[100];
    snprintf(url, sizeof(url), "%s/%s", CLAIM_BASE_URL, path);
    esp_http_client_config_t config = {
        .url = url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .buffer_size = 1024,
        .cert_pem = (const char *)claiming_server_root_ca_pem_start,
        .skip_cert_common_name_check = false
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialise HTTP Client.");
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Payload for %s: %s", url, claim_data->payload);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_err_t err = esp_http_client_open(client, strlen(claim_data->payload));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open connection to %s", url);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    int len = esp_http_client_write(client, claim_data->payload, strlen(claim_data->payload));
    if (len != strlen(claim_data->payload)) {
        ESP_LOGE(TAG, "Failed to write Payload. Returned len = %d.", len);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Wrote %d of %d bytes.", len, strlen(claim_data->payload));
    len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if ((len > 0) && (status == 200)) {
        len = esp_http_client_read_response(client, claim_data->payload, sizeof(claim_data->payload));
        claim_data->payload[len] = '\0';
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Invalid response for %s. Status = %d, Content Length = %d.",url, status, len);
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_FAIL;
}
static esp_err_t esp_rmaker_claim_perform_init(esp_rmaker_claim_data_t *claim_data)
{
    esp_err_t err = esp_rmaker_claim_perform_common(claim_data, CLAIM_INIT_PATH);
    if (err != OK) {
        ESP_LOGE(TAG, "Claim Init Request Failed.");
        return err;
    }
    err = handle_claim_init_response(claim_data);
    return err;
}

static esp_err_t esp_rmaker_claim_perform_verify(esp_rmaker_claim_data_t *claim_data)
{
    esp_err_t err = esp_rmaker_claim_perform_common(claim_data, CLAIM_VERIFY_PATH);
    if (err != OK) {
        ESP_LOGE(TAG, "Claim Verify Failed.");
        return err;
    }
    err = handle_claim_verify_response(claim_data);
    return err;
}

esp_err_t esp_rmaker_self_claim_perform(void)
{
    ESP_LOGI(TAG, "Starting the Self Claim Process. This may take time.");
    if (g_claim_data == NULL) {
        ESP_LOGE(TAG, "Self claiming not initialised.");
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = esp_rmaker_claim_perform_init(g_claim_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Claim Init Sequence Failed.");
        return err;
    }
    err = esp_rmaker_claim_perform_verify(g_claim_data);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Self Claiming was successful. Certificate received.");
        ESP_LOGW(TAG, "The first MQTT connection attempt may fail. The subsequent ones should work.");
    }
    free(g_claim_data);
    g_claim_data = NULL;
    return err;
}

static esp_err_t generate_key(esp_rmaker_claim_data_t *claim_data)
{
    const char *pers = "gen_key";
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;

    mbedtls_pk_free(&claim_data->key);
    mbedtls_pk_init(&claim_data->key);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    memset(claim_data->payload, 0, sizeof(claim_data->payload));

    ESP_LOGD(TAG, "Seeding the random number generator.");
    mbedtls_entropy_init(&entropy);
    int ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, strlen(pers));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned -0x%04x", -ret );
        mbedtls_pk_free(&claim_data->key);
        goto exit;
    }

    ESP_LOGD(TAG, "Generating the private key..." );
    ret = mbedtls_pk_setup(&claim_data->key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_setup returned -0x%04x", -ret );
        mbedtls_pk_free(&claim_data->key);
        goto exit;
    }

    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(claim_data->key), mbedtls_ctr_drbg_random, &ctr_drbg, CLAIM_PK_SIZE, 65537); /* here, 65537 is the RSA exponent */
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_rsa_gen_key returned -0x%04x", -ret );
        mbedtls_pk_free(&claim_data->key);
        goto exit;
    }

    ESP_LOGD(TAG, "Converting Private Key to PEM...");
    ret = mbedtls_pk_write_key_pem(&claim_data->key, (unsigned char *)claim_data->payload, sizeof(claim_data->payload));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_pk_write_key_pem returned -0x%04x", -ret );
        mbedtls_pk_free(&claim_data->key);
        goto exit;
    }
exit:
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    return ret;
}

static esp_err_t generate_csr(esp_rmaker_claim_data_t *claim_data)
{
    const char *pers = "gen_csr";
    mbedtls_x509write_csr csr;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    /* Generating CSR from the private key */
    mbedtls_x509write_csr_init(&csr);
    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    ESP_LOGD(TAG, "Seeding the random number generator.");
    mbedtls_entropy_init(&entropy);
    int ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy, (const unsigned char *) pers, strlen(pers));
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned -0x%04x", -ret );
        goto exit;
    }
    char subject_name[20];
    snprintf(subject_name, sizeof(subject_name), "CN=%s", esp_rmaker_get_node_id());
    ret = mbedtls_x509write_csr_set_subject_name(&csr, subject_name);
    if (ret != 0) {
        ESP_LOGE(TAG, "mbedtls_x509write_csr_set_subject_name returned %d", ret );
        goto exit;
    }

    memset(claim_data->csr, 0, sizeof(claim_data->csr));
    mbedtls_x509write_csr_set_key(&csr, &claim_data->key);
    ESP_LOGD(TAG, "Generating PEM");
    ret = mbedtls_x509write_csr_pem(&csr, claim_data->csr, sizeof(claim_data->csr), mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret < 0) {
        ESP_LOGE(TAG, "mbedtls_x509write_csr_pem returned -0x%04x", -ret );
        goto exit;
    }
    ESP_LOGD(TAG, "CSR generated.");
exit:

    mbedtls_x509write_csr_free(&csr);
    mbedtls_pk_free(&claim_data->key);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    return ret;
}

esp_err_t esp_rmaker_self_claim_init(void)
{
    ESP_LOGI(TAG, "Initialising Self Claiming. This may take time.");
    /* Check if the claim data structure is already allocated. If yes, free it */
    if (g_claim_data) {
        free(g_claim_data);
        g_claim_data = NULL;
    }
    /* Allocate memory for the claim data */
    g_claim_data = calloc(1, sizeof(esp_rmaker_claim_data_t));
    if (!g_claim_data) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes for Claim data.", sizeof(esp_rmaker_claim_data_t));
        return ESP_ERR_NO_MEM;
    }
    /* Check if the CSR is already available. If yes, just generate the payload for claim init
     * and return from here instead of re-generating the key and CSR
     */
    void *csr = esp_rmaker_get_client_csr();
    if (csr) {
        snprintf(g_claim_data->payload, MAX_PAYLOAD_SIZE, "{\"device-id\":\"%s\",\"csr\":\"%s\"}",
                esp_rmaker_get_node_id(), (char *)csr);
        free(csr);
        ESP_LOGI(TAG, "CSR already exists. No need to re-initialise Claiming.");
        return ESP_OK;
    }
    /* Generate the Private Key */
    esp_err_t err = generate_key(g_claim_data);
    if (err != ESP_OK) {
        free(g_claim_data);
        g_claim_data = NULL;
        ESP_LOGE(TAG, "Failed to generate private key.");
        return err;
    }
    /* Store the key in the storage */
    ESP_LOGD(TAG, "Storing private key to NVS...");
    err = esp_rmaker_storage_set(ESP_RMAKER_CLIENT_KEY_NVS_KEY, g_claim_data->payload, strlen((char *)g_claim_data->payload));
    if (err != ESP_OK) {
         ESP_LOGE(TAG, "Failed to store private key to storage.");
        free(g_claim_data);
        g_claim_data = NULL;
    }

    /* Generate CSR */
    err = generate_csr(g_claim_data);
    ESP_LOGD(TAG, "CSR generated successfully.");
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate CSR for Claiming");
        free(g_claim_data);
        g_claim_data = NULL;
        return err;
    }

    /* New line characters from the CSR need to be removed and replaced with explicit \r\n for thr claiming
     * service to parse properly. Make that change here and store the CSR in storage.
     */
    escape_new_line(g_claim_data);
    err = esp_rmaker_storage_set(ESP_RMAKER_CLIENT_CSR_NVS_KEY, g_claim_data->csr, strlen((char *)g_claim_data->csr));
    if (err != ESP_OK) {
        free(g_claim_data);
        g_claim_data = NULL;
        return err;
    } else {
        ESP_LOGI(TAG, "Self Claiming initialised successfully.");
    }
    snprintf(g_claim_data->payload, MAX_PAYLOAD_SIZE, "{\"device-id\":\"%s\",\"csr\":\"%s\"}",
            esp_rmaker_get_node_id(), g_claim_data->csr);
    return err;
}

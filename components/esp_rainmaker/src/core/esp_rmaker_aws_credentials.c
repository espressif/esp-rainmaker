/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_log.h>
#include <inttypes.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_factory.h>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include <json_parser.h>
#include <time.h>
#include <esp_rmaker_utils.h>
#include "esp_rmaker_client_data.h"

static const char *TAG = "rmaker_aws_creds";

/* NVS key for credential endpoint */
#define RMAKER_CREDENTIAL_ENDPOINT_KEY "mqtt_cred_host"

/* https://docs.aws.amazon.com/accounts/latest/reference/API_Region.html */
#define MAX_AWS_REGION_LENGTH 50

/* Typical response size is less than 4096 bytes: ~3K */
#define HTTP_CLIENT_RECV_BUF_SIZE 4096

/* Current time threshold to check if system time is synchronized */
#define CURRENT_TIME_THRESHOLD ((time_t)(2025 - 1970) * 365 * 24 * 60 * 60 + (2025 - 1970) / 4 * 24 * 60 * 60) /* Jan 1, 2025 */

/* Default AWS region to use if extraction fails */
#define DEFAULT_AWS_REGION "us-east-1"

/* HTTP response collector context and handler */
typedef struct {
    char* buf;
    uint32_t cur;
    uint32_t max;
} http_resp_ctx_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_ctx_t *ctx = (http_resp_ctx_t *) evt->user_data;
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            if (ctx) {
                ctx->cur = 0;
                if (ctx->buf && ctx->max > 0) {
                    ctx->buf[0] = '\0';
                }
            }
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGV(TAG, "HDR %s: %s", evt->header_key ? evt->header_key : "null",
                     evt->header_value ? evt->header_value : "null");
            break;
        case HTTP_EVENT_ON_DATA:
            if (ctx && ctx->buf && evt->data && evt->data_len > 0) {
                uint32_t remaining = (ctx->max > ctx->cur) ? (ctx->max - ctx->cur) : 0;
                uint32_t copy = (remaining > 1 && (uint32_t)evt->data_len < remaining) ? (uint32_t)evt->data_len : (remaining > 1 ? remaining - 1 : 0);
                if (copy > 0) {
                    memcpy(ctx->buf + ctx->cur, evt->data, copy);
                    ctx->cur += copy;
                    ctx->buf[ctx->cur] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH (len=%" PRIu32 ")", ctx ? ctx->cur : 0);
            if (ctx && ctx->buf && ctx->max > 0) {
                ctx->buf[(ctx->cur < ctx->max) ? ctx->cur : (ctx->max - 1)] = '\0';
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGW(TAG, "HTTP_EVENT_REDIRECT");
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* Get AWS region from credential endpoint */
char* esp_rmaker_get_aws_region(void)
{
    /* Get credential endpoint from factory */
    bool need_cred_ep_free = true;
    char *credential_endpoint = (char *) esp_rmaker_factory_get(RMAKER_CREDENTIAL_ENDPOINT_KEY);
    if (!credential_endpoint) {
#ifdef CONFIG_ESP_RMAKER_MQTT_CRED_HOST
        ESP_LOGW(TAG, "Credential endpoint not found in factory, using one from config");
        need_cred_ep_free = false;
        credential_endpoint = (char *) CONFIG_ESP_RMAKER_MQTT_CRED_HOST;
#else
        ESP_LOGW(TAG, "Credential endpoint not found in factory, using default region: %s", DEFAULT_AWS_REGION);
        char *region = MEM_ALLOC_EXTRAM(strlen(DEFAULT_AWS_REGION) + 1);
        if (region) {
            strcpy(region, DEFAULT_AWS_REGION);
        }
        return region;
#endif
    }

    char *region = NULL;

    /* Extract the AWS region from the credential endpoint
     * Format: something.iot.REGION.amazonaws.com -> REGION */
    char *region_start = strstr(credential_endpoint, ".iot.");
    if (region_start) {
        region_start += strlen(".iot.");
        char *region_end = strchr(region_start, '.');
        size_t region_len = region_end ? (size_t)(region_end - region_start) : strlen(region_start);

        if (region_len > 0 && region_len < MAX_AWS_REGION_LENGTH) {
            region = MEM_ALLOC_EXTRAM(region_len + 1);
            if (region) {
                strncpy(region, region_start, region_len);
                region[region_len] = '\0';
                ESP_LOGI(TAG, "Extracted AWS region from endpoint: %s", region);
            }
        }
    }

    /* If extraction failed, use default */
    if (!region) {
        ESP_LOGW(TAG, "Failed to extract region from endpoint: %s, using default: %s",
                 credential_endpoint, DEFAULT_AWS_REGION);
        region = MEM_ALLOC_EXTRAM(strlen(DEFAULT_AWS_REGION) + 1);
        if (region) {
            strcpy(region, DEFAULT_AWS_REGION);
        }
    }

    if (need_cred_ep_free) {
        free(credential_endpoint);
    }
    return region;
}

/* Assume the given role, obtain the aws security token */
esp_rmaker_aws_credentials_t* esp_rmaker_get_aws_security_token(const char *role_alias)
{
    /* Declare all variables that need cleanup at function scope */
    char *resp = NULL;
    char *client_cert_pem = NULL;
    char *client_key_pem = NULL;
    esp_http_client_handle_t h = NULL;
    esp_rmaker_aws_credentials_t *credentials = NULL;

    /* Build URL: https://<endpoint>/role-aliases/<roleAlias>/credentials */
    char url[256];

    bool need_cred_ep_free = true;
    char *credential_endpoint = (char *) esp_rmaker_factory_get(RMAKER_CREDENTIAL_ENDPOINT_KEY);
    if (credential_endpoint == NULL) {
#ifdef CONFIG_ESP_RMAKER_MQTT_CRED_HOST
        ESP_LOGW(TAG, "Credential endpoint not found in factory, using one from config");
        credential_endpoint = (char *) CONFIG_ESP_RMAKER_MQTT_CRED_HOST;
        need_cred_ep_free = false;
#else
        ESP_LOGE(TAG, "Failed to get credential endpoint from factory and not found in config");
        goto cleanup_early;
#endif
    }

    snprintf(url, sizeof(url), "https://%s/role-aliases/%s/credentials",
            credential_endpoint, role_alias);

    /* Free credential endpoint after use */
    if (need_cred_ep_free) {
        free(credential_endpoint);
    }

    /* Response buffer and context */
    /* Allocate response buffer on heap, preferring external RAM */
    resp = MEM_CALLOC_EXTRAM(1, HTTP_CLIENT_RECV_BUF_SIZE);
    if (!resp) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        goto cleanup_early;
    }
    http_resp_ctx_t ctx = {
        .buf = resp,
        .cur = 0,
        .max = HTTP_CLIENT_RECV_BUF_SIZE
    };

    /* Configure mutual TLS client */
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = http_event_handler,
        .user_data = &ctx,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    /* Load PEM contents directly from RainMaker factory */
    client_cert_pem = (char *) esp_rmaker_factory_get(ESP_RMAKER_CLIENT_CERT_NVS_KEY);
    client_key_pem  = (char *) esp_rmaker_factory_get(ESP_RMAKER_CLIENT_KEY_NVS_KEY);
    if ((client_cert_pem == NULL) || (client_key_pem == NULL)) {
        ESP_LOGE(TAG, "Failed to get client cert/key from factory");
        goto cleanup_certs_with_resp;
    }
    cfg.client_cert_pem = client_cert_pem;
    cfg.client_key_pem  = client_key_pem;

    h = esp_http_client_init(&cfg);
    if (!h) {
        goto cleanup_certs_with_resp;
    }

    /* Set thing name header */
    const char *thing_name = esp_rmaker_get_node_id();
    if (thing_name) {
        esp_http_client_set_header(h, "x-amzn-iot-thingname", thing_name);
    }
    esp_http_client_set_header(h, "accept", "*/*");

    esp_err_t err = esp_http_client_perform(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP perform failed: %s", esp_err_to_name(err));
        goto cleanup_http_with_resp;
    }
    int status = esp_http_client_get_status_code(h);
    if ((status < 200) || (status >= 300)) {
        ESP_LOGE(TAG, "HTTP status: %d, body: %s", status, resp);
        goto cleanup_http_with_resp;
    }
    esp_http_client_cleanup(h);
    h = NULL;

    /* Free factory strings after client cleanup */
    free(client_cert_pem);
    free(client_key_pem);
    client_cert_pem = NULL;
    client_key_pem = NULL;

    ESP_LOGI(TAG, "Response: %s", resp);

    /* Parse response JSON using json_parser */
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, resp, strlen(resp)) != 0) {
        ESP_LOGE(TAG, "JSON parse start failed");
        goto cleanup_resp_only;
    }

    /* The AWS IoT response wraps credentials under "credentials" object
     * Try lower-case key first, then PascalCase as fallback */
    if (json_obj_get_object(&jctx, "credentials") != 0) {
        if (json_obj_get_object(&jctx, "Credentials") != 0) {
            ESP_LOGE(TAG, "JSON parse: credentials object not found");
            json_parse_end(&jctx);
            goto cleanup_resp_only;
        }
    }

    /* Allocate the credentials structure early, preferring external RAM */
    credentials = MEM_CALLOC_EXTRAM(1, sizeof(esp_rmaker_aws_credentials_t));
    if (!credentials) {
        ESP_LOGE(TAG, "Failed to allocate memory for credentials");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_resp_only;
    }

    /* Get exact string lengths and allocate directly to structure members */
    int ak_len = 0, sk_len = 0, tok_len = 0;

    if (json_obj_get_strlen(&jctx, "accessKeyId", &ak_len) != 0 || ak_len <= 0) {
        ESP_LOGE(TAG, "Failed to get accessKeyId length");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }

    if (json_obj_get_strlen(&jctx, "secretAccessKey", &sk_len) != 0 || sk_len <= 0) {
        ESP_LOGE(TAG, "Failed to get secretAccessKey length");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }

    if (json_obj_get_strlen(&jctx, "sessionToken", &tok_len) != 0 || tok_len <= 0) {
        ESP_LOGE(TAG, "Failed to get sessionToken length");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }

    /* Allocate exact sizes for credential strings, preferring external RAM */
    credentials->access_key = MEM_ALLOC_EXTRAM(ak_len + 1);
    credentials->secret_key = MEM_ALLOC_EXTRAM(sk_len + 1);
    credentials->session_token = MEM_ALLOC_EXTRAM(tok_len + 1);

    if (!credentials->access_key || !credentials->secret_key || !credentials->session_token) {
        ESP_LOGE(TAG, "Failed to allocate memory for credential strings");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }

    /* Parse directly into the allocated structure members */
    if (json_obj_get_string(&jctx, "accessKeyId", credentials->access_key, ak_len + 1) != 0) {
        ESP_LOGE(TAG, "JSON parse accessKeyId failed");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }
    if (json_obj_get_string(&jctx, "secretAccessKey", credentials->secret_key, sk_len + 1) != 0) {
        ESP_LOGE(TAG, "JSON parse secretAccessKey failed");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }
    if (json_obj_get_string(&jctx, "sessionToken", credentials->session_token, tok_len + 1) != 0) {
        ESP_LOGE(TAG, "JSON parse sessionToken failed");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }

    /* Set the lengths */
    credentials->access_key_len = (uint32_t)ak_len;
    credentials->secret_key_len = (uint32_t)sk_len;
    credentials->session_token_len = (uint32_t)tok_len;

    /* Retrieve expiration length and string */
    int exp_strlen = 0;
    if ((json_obj_get_strlen(&jctx, "expiration", &exp_strlen) != 0) ||
        (exp_strlen <= 0) || (exp_strlen >= 64)) {
        ESP_LOGE(TAG, "JSON parse expiration length failed");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }
    char exp_buf[64];
    if (json_obj_get_string(&jctx, "expiration", exp_buf, sizeof(exp_buf)) != 0) {
        ESP_LOGE(TAG, "JSON parse expiration failed");
        json_obj_leave_object(&jctx);
        json_parse_end(&jctx);
        goto cleanup_credentials_and_resp;
    }

    /* Leave credentials object */
    json_obj_leave_object(&jctx);
    json_parse_end(&jctx);

    time_t parsed_expiration = 0;
    if (esp_rmaker_time_convert_iso8601_to_epoch(exp_buf, strlen(exp_buf), &parsed_expiration) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to convert ISO8601 to epoch");
        goto cleanup_credentials_and_resp;
    }
    ESP_LOGI(TAG, "Parsed expiration: %ld", (long)parsed_expiration);

    /* Calculate relative expiration (seconds from now) */
    time_t current_time = time(NULL);
    ESP_LOGI(TAG, "Current system time: %ld", (long)current_time);

    /* Check if system time seems reasonable (after current time threshold) */
    if (current_time < CURRENT_TIME_THRESHOLD) {
        ESP_LOGW(TAG, "System time not synchronized, using default credential lifetime");
        /* Typically, the credential lifetime is 1 hour (i.e., 3600 seconds) */
        credentials->expiration = 3600;
        ESP_LOGI(TAG, "Using default credential lifetime: 3600 seconds");
    } else {
        time_t relative_expiration = parsed_expiration - current_time;

        if (relative_expiration <= 0) {
            ESP_LOGW(TAG, "Credentials already expired or expiring immediately");
            relative_expiration = 0;
        }

        ESP_LOGI(TAG, "Credentials expire in %" PRIu32 " seconds", (uint32_t) relative_expiration);
        credentials->expiration = (uint32_t) relative_expiration;
    }

    /* Free response buffer before returning */
    free(resp);

    return credentials;

cleanup_credentials_and_resp:
    if (credentials) {
        esp_rmaker_free_aws_credentials(credentials);
    }
cleanup_resp_only:
    if (resp) {
        free(resp);
    }
    return NULL;

cleanup_http_with_resp:
    if (h) {
        esp_http_client_cleanup(h);
    }
cleanup_certs_with_resp:
    if (client_cert_pem) {
        free(client_cert_pem);
    }
    if (client_key_pem) {
        free(client_key_pem);
    }
    if (resp) {
        free(resp);
    }
cleanup_early:
    return NULL;
}

/* Free AWS credentials structure */
void esp_rmaker_free_aws_credentials(esp_rmaker_aws_credentials_t *credentials)
{
    if (credentials) {
        if (credentials->access_key) {
            free(credentials->access_key);
        }
        if (credentials->secret_key) {
            free(credentials->secret_key);
        }
        if (credentials->session_token) {
            free(credentials->session_token);
        }
        free(credentials);
    }
}

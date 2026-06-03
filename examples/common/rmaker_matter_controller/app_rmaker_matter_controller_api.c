/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cJSON.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_utils.h>
#include <mbedtls/base64.h>
#include <mbedtls/pem.h>
#include <stdint.h>
#include <stdlib.h>

#include "app_rmaker_matter_controller.h"
#include "app_rmaker_matter_controller_api.h"

#define TAG "rmaker_matter_controller_api"
#define RAINMAKER_URL_LEN 256

#ifdef CONFIG_RM_USER_SUPPORT_REUSE_HTTP_SESSION
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
#define MATTER_CONTROLLER_REUSE_SESSION true
#endif
#endif

// ==================== Matter Controller APIs ====================

#define PEM_BEGIN_CSR "-----BEGIN CERTIFICATE REQUEST-----\n"
#define PEM_END_CSR "-----END CERTIFICATE REQUEST-----\n"

/**
 * @brief Convert character to hex digit
 */
static int convert_char_to_digit(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    } else if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    } else if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

#ifdef CONFIG_NEWLIB_NANO_FORMAT
/**
 * @brief Convert hex string to uint64_t
 */
static uint64_t convert_hex_string_to_uint64(const char *str)
{
    if (!str) {
        return 0;
    }
    uint64_t value = 0;
    size_t len = strnlen(str, 16);
    for (size_t i = 0; i < len; i++) {
        int digit = convert_char_to_digit(str[i]);
        if (digit == -1) {
            return 0;
        }
        value = (value << 4) | (uint8_t)digit;
    }
    return value;
}
#else
static uint64_t convert_hex_string_to_uint64(const char *str)
{
    return strtoull(str, NULL, 16);
}
#endif

/**
 * @brief Convert hex string to bytes
 */
static bool convert_hex_string_to_bytes(const char *str, uint8_t *bytes, size_t bytes_len)
{
    if (strlen(str) != bytes_len * 2) {
        return false;
    }
    for (size_t i = 0; i < bytes_len; ++i) {
        int byte_h = convert_char_to_digit(str[2 * i]);
        int byte_l = convert_char_to_digit(str[2 * i + 1]);
        if (byte_h < 0 || byte_l < 0) {
            return false;
        }
        bytes[i] = (((uint8_t)byte_h) << 4) + (uint8_t)byte_l;
    }
    return true;
}

/**
 * @brief Deformat certificate (replace \n with newlines)
 */
static char *deformat_cert(const char *input)
{
    size_t len = strlen(input);
    char *output = MEM_CALLOC_EXTRAM(1, len + 1);
    if (!output) {
        return NULL;
    }
    size_t output_len = 0;
    while (*input) {
        if (*input == '\\' && *(input + 1) == 'n') {
            output[output_len++] = '\n';
            input += 2;
        } else {
            output[output_len++] = *input;
            input++;
        }
    }
    output[output_len] = '\0';
    return output;
}

/**
 * @brief Convert DER to PEM format
 */
static esp_err_t convert_der_to_pem(const uint8_t *der, size_t der_len, char *pem_buf, size_t pem_buf_size)
{
    if (!der || der_len == 0 || !pem_buf || pem_buf_size <= der_len) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t pem_len = pem_buf_size;
    if (mbedtls_pem_write_buffer(PEM_BEGIN_CSR, PEM_END_CSR, der, der_len, (unsigned char *)pem_buf, pem_buf_size,
                                 &pem_len) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief Convert PEM to DER format
 */
static esp_err_t convert_pem_to_der(const char *pem, uint8_t *der_buf, size_t *der_len)
{
    if (!pem || strlen(pem) == 0 || !der_buf || !der_len) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t pem_len = strlen(pem);
    const char *s1 = strstr(pem, "-----BEGIN");
    if (!s1) {
        return ESP_FAIL;
    }
    const char *s2 = strstr(pem, "-----END");
    if (!s2) {
        return ESP_FAIL;
    }
    const char *end = pem + pem_len;
    s1 += 10;
    while (s1 < end && *s1 != '-') s1++;
    while (s1 < end && *s1 == '-') s1++;
    if (*s1 == '\r')
        s1++;
    if (*s1 == '\n')
        s1++;

    size_t len = 0;
    int ret = mbedtls_base64_decode(NULL, 0, &len, (const unsigned char *)s1, s2 - s1);
    if (ret == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
        return ESP_FAIL;
    }
    if (len > *der_len) {
        return ESP_ERR_NO_MEM;
    }
    if (mbedtls_base64_decode(der_buf, len, &len, (const unsigned char *)s1, s2 - s1) != 0) {
        return ESP_FAIL;
    }
    *der_len = len;
    return ESP_OK;
}

// /v1/user/node_group - Get group ID by fabric ID
esp_err_t app_rmaker_api_get_group_id_by_fabric(uint64_t fabric_id, char *group_id, size_t group_id_len)
{
    if (!group_id || group_id_len == 0) {
        ESP_LOGE(TAG, "group_id is NULL or group_id_len is 0");
        return ESP_ERR_INVALID_ARG;
    }

    app_rmaker_user_api_request_config_t request_config = {
#ifdef MATTER_CONTROLLER_REUSE_SESSION
        .reuse_session = true,
#else
        .reuse_session = false,
#endif
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/node_group",
        .api_version = NULL,
        .api_query_params = "node_list=false&sub_groups=false&node_details=false&is_matter=true&matter_node_list=false&"
                            "fabric_details=false",
        .api_payload = NULL,
    };

    char *response_data = NULL;
    int status_code = 0;
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        free(response_data);
        return err;
    }
    cJSON *response = cJSON_Parse(response_data);
    free(response_data);

    if (!response) {
        return ESP_FAIL;
    }

    cJSON *groups = cJSON_GetObjectItem(response, "groups");
    if (groups && cJSON_IsArray(groups)) {
        int count = cJSON_GetArraySize(groups);
        for (int i = 0; i < count; i++) {
            cJSON *group = cJSON_GetArrayItem(groups, i);
            cJSON *fid = cJSON_GetObjectItem(group, "fabric_id");
            if (fid && fid->valuestring) {
                if (convert_hex_string_to_uint64(fid->valuestring) == fabric_id) {
                    cJSON *gid = cJSON_GetObjectItem(group, "group_id");
                    if (gid && gid->valuestring) {
                        strncpy(group_id, gid->valuestring, group_id_len - 1);
                        group_id[group_id_len - 1] = '\0';
                        cJSON_Delete(response);
                        return ESP_OK;
                    }
                }
            }
        }
    }
    cJSON_Delete(response);

    return ESP_ERR_NOT_FOUND;
}

// /v1/user/node_group - Get matter fabric ID
esp_err_t app_rmaker_api_get_matter_fabric_id(const char *group_id, uint64_t *fabric_id)
{
    if (!group_id || !fabric_id) {
        ESP_LOGE(TAG, "group_id or fabric_id is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char query_parameters[RAINMAKER_URL_LEN] = {0};
    snprintf(query_parameters, sizeof(query_parameters),
             "group_id=%s&node_list=false&sub_groups=false&node_details=false&is_matter=true&fabric_details=false",
             group_id);

    app_rmaker_user_api_request_config_t request_config = {
#ifdef MATTER_CONTROLLER_REUSE_SESSION
        .reuse_session = true,
#else
        .reuse_session = false,
#endif
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/node_group",
        .api_version = NULL,
        .api_query_params = query_parameters,
        .api_payload = NULL,
    };

    char *response_data = NULL;
    int status_code = 0;
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        free(response_data);
        return err;
    }
    cJSON *response = cJSON_Parse(response_data);
    free(response_data);

    if (!response) {
        return ESP_FAIL;
    }

    err = ESP_FAIL;
    cJSON *groups = cJSON_GetObjectItem(response, "groups");
    if (groups && cJSON_IsArray(groups) && cJSON_GetArraySize(groups) == 1) {
        cJSON *group = cJSON_GetArrayItem(groups, 0);
        cJSON *fid = cJSON_GetObjectItem(group, "fabric_id");
        if (fid && fid->valuestring && strlen(fid->valuestring) == 16) {
            *fabric_id = convert_hex_string_to_uint64(fid->valuestring);
            if (*fabric_id != 0) {
                err = ESP_OK;
            }
        }
    }
    cJSON_Delete(response);
    return err;
}

// /v1/user/node_group - Get fabric RCAC
esp_err_t app_rmaker_api_get_fabric_rcac(const char *group_id, unsigned char *rcac_der, size_t *rcac_der_len)
{
    if (!group_id || !rcac_der || !rcac_der_len) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    char query_parameters[RAINMAKER_URL_LEN] = {0};
    snprintf(query_parameters, sizeof(query_parameters),
             "group_id=%s&node_list=false&sub_groups=false&node_details=false&is_matter=true&fabric_details=true",
             group_id);

    app_rmaker_user_api_request_config_t request_config = {
#ifdef MATTER_CONTROLLER_REUSE_SESSION
        .reuse_session = true,
#else
        .reuse_session = false,
#endif
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/node_group",
        .api_version = NULL,
        .api_query_params = query_parameters,
        .api_payload = NULL,
    };

    char *response_data = NULL;
    int status_code = 0;
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        free(response_data);
        return err;
    }
    cJSON *response = cJSON_Parse(response_data);
    free(response_data);

    if (!response) {
        return ESP_FAIL;
    }

    err = ESP_FAIL;
    cJSON *groups = cJSON_GetObjectItem(response, "groups");
    if (groups && cJSON_IsArray(groups) && cJSON_GetArraySize(groups) == 1) {
        cJSON *group = cJSON_GetArrayItem(groups, 0);
        cJSON *fabric_details = cJSON_GetObjectItem(group, "fabric_details");
        if (fabric_details) {
            cJSON *root_ca = cJSON_GetObjectItem(fabric_details, "root_ca");
            if (root_ca && root_ca->valuestring) {
                char *rcac_pem = deformat_cert(root_ca->valuestring);
                if (rcac_pem) {
                    err = convert_pem_to_der(rcac_pem, rcac_der, rcac_der_len);
                    free(rcac_pem);
                }
            }
        }
    }
    cJSON_Delete(response);
    return err;
}

// /v1/user/node_group - Get fabric IPK
esp_err_t app_rmaker_api_get_fabric_ipk(const char *group_id, uint8_t *ipk_buf, size_t ipk_buf_size)
{
    if (!group_id || !ipk_buf || ipk_buf_size < ESP_MATTER_IPK_LEN) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    char query_parameters[RAINMAKER_URL_LEN] = {0};
    snprintf(query_parameters, sizeof(query_parameters),
             "group_id=%s&node_list=false&sub_groups=false&node_details=false&is_matter=true&fabric_details=true",
             group_id);

    app_rmaker_user_api_request_config_t request_config = {
#ifdef MATTER_CONTROLLER_REUSE_SESSION
        .reuse_session = true,
#else
        .reuse_session = false,
#endif
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/node_group",
        .api_version = NULL,
        .api_query_params = query_parameters,
        .api_payload = NULL,
    };

    char *response_data = NULL;
    int status_code = 0;
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        free(response_data);
        return err;
    }
    cJSON *response = cJSON_Parse(response_data);
    free(response_data);

    if (!response) {
        return ESP_FAIL;
    }

    err = ESP_FAIL;
    cJSON *groups = cJSON_GetObjectItem(response, "groups");
    if (groups && cJSON_IsArray(groups) && cJSON_GetArraySize(groups) == 1) {
        cJSON *group = cJSON_GetArrayItem(groups, 0);
        cJSON *fabric_details = cJSON_GetObjectItem(group, "fabric_details");
        if (fabric_details) {
            cJSON *ipk = cJSON_GetObjectItem(fabric_details, "ipk");
            if (ipk && ipk->valuestring && strlen(ipk->valuestring) == ESP_MATTER_IPK_LEN * 2) {
                if (convert_hex_string_to_bytes(ipk->valuestring, ipk_buf, ESP_MATTER_IPK_LEN)) {
                    err = ESP_OK;
                }
            }
        }
    }
    cJSON_Delete(response);
    return err;
}

// /v1/user/node_group - Issue NOC with CSR
esp_err_t app_rmaker_api_issue_noc(const uint8_t *csr_der, size_t csr_der_len, const char *group_id,
                                   uint64_t *matter_node_id, uint8_t *noc_der, size_t *noc_der_len)
{
    if (!csr_der || csr_der_len == 0 || !group_id || !matter_node_id || !noc_der || !noc_der_len) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // Convert CSR DER to PEM
    char *csr_pem = MEM_CALLOC_EXTRAM(1, 1024);
    if (!csr_pem) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = convert_der_to_pem(csr_der, csr_der_len, csr_pem, 1024);
    if (err != ESP_OK) {
        free(csr_pem);
        return err;
    }

    // Create request payload
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        free(csr_pem);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "operation", "add");
    cJSON *csr_requests = cJSON_AddArrayToObject(root, "csr_requests");
    cJSON *csr_obj = cJSON_CreateObject();
    {
        cJSON_AddStringToObject(csr_obj, "role", "primary");
        char node_id_str[17];
        uint64_t node_id = *matter_node_id;
        // Use two 32-bit hex parts, avoids PRIX64/llX portability issues
        snprintf(node_id_str, sizeof(node_id_str), "%08lX%08lX", (uint32_t)(node_id >> 32),
                 (uint32_t)(node_id & 0xFFFFFFFF));
        cJSON_AddStringToObject(csr_obj, "matter_node_id", node_id_str);
    }
    cJSON_AddStringToObject(csr_obj, "group_id", group_id);
    cJSON_AddStringToObject(csr_obj, "csr", csr_pem);
    cJSON_AddItemToArray(csr_requests, csr_obj);
    cJSON_AddStringToObject(root, "csr_type", "controller");

    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    free(csr_pem);

    if (!payload) {
        return ESP_ERR_NO_MEM;
    }

    app_rmaker_user_api_request_config_t request_config = {
#ifdef MATTER_CONTROLLER_REUSE_SESSION
        .reuse_session = true,
#else
        .reuse_session = false,
#endif
        .api_type = APP_RMAKER_USER_API_TYPE_PUT,
        .api_name = "user/node_group",
        .api_version = NULL,
        .api_query_params = NULL,
        .api_payload = payload,
    };

    char *response_data = NULL;
    int status_code = 0;
    err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    free(payload);
    if (err != ESP_OK) {
        free(response_data);
        return err;
    }
    cJSON *response = cJSON_Parse(response_data);
    free(response_data);

    if (!response) {
        return ESP_FAIL;
    }

    err = ESP_FAIL;
    cJSON *certs = cJSON_GetObjectItem(response, "certificates");
    if (certs && cJSON_IsArray(certs) && cJSON_GetArraySize(certs) == 1) {
        cJSON *cert = cJSON_GetArrayItem(certs, 0);
        cJSON *user_noc = cJSON_GetObjectItem(cert, "user_noc");
        if (user_noc && user_noc->valuestring) {
            char *noc_pem = deformat_cert(user_noc->valuestring);
            if (noc_pem) {
                err = convert_pem_to_der(noc_pem, noc_der, noc_der_len);
                free(noc_pem);
            }
        }
    }
    cJSON_Delete(response);
    return err;
}

// /v1/user/node_group - Create Matter controller
esp_err_t app_rmaker_api_create_matter_controller(const char *rainmaker_node_id, const char *group_id,
                                                  uint64_t *matter_node_id)
{
    if (!rainmaker_node_id || !group_id || !matter_node_id) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "node_id", rainmaker_node_id);
    cJSON_AddStringToObject(root, "group_id", group_id);
    char *payload = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!payload) {
        return ESP_ERR_NO_MEM;
    }

    app_rmaker_user_api_request_config_t request_config = {
#ifdef MATTER_CONTROLLER_REUSE_SESSION
        .reuse_session = true,
#else
        .reuse_session = false,
#endif
        .api_type = APP_RMAKER_USER_API_TYPE_PUT,
        .api_name = "user/node_group",
        .api_version = NULL,
        .api_query_params = "matter_controller=true",
        .api_payload = payload,
    };

    char *response_data = NULL;
    int status_code = 0;
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    free(payload);
    if (err != ESP_OK) {
        free(response_data);
        return err;
    }
    cJSON *response = cJSON_Parse(response_data);
    free(response_data);

    if (!response) {
        return ESP_FAIL;
    }

    err = ESP_FAIL;
    cJSON *status = cJSON_GetObjectItem(response, "status");
    if (status && status->valuestring && strcmp(status->valuestring, "success") == 0) {
        cJSON *node_id = cJSON_GetObjectItem(response, "matter_node_id");
        if (node_id && node_id->valuestring) {
            *matter_node_id = convert_hex_string_to_uint64(node_id->valuestring);
            err = ESP_OK;
        }
    }
    cJSON_Delete(response);
    return err;
}

static esp_err_t fetch_matter_node_list(const char *group_id, matter_device_t **device_list)
{
    char query_parameters[RAINMAKER_URL_LEN] = {0};
    snprintf(query_parameters, sizeof(query_parameters),
             "group_id=%s&node_details=false&sub_groups=false&node_list=true&is_matter=true&matter_node_list=true",
             group_id);

    app_rmaker_user_api_request_config_t request_config = {
#ifdef MATTER_CONTROLLER_REUSE_SESSION
        .reuse_session = true,
#else
        .reuse_session = false,
#endif
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/node_group",
        .api_version = NULL,
        .api_query_params = query_parameters,
        .api_payload = NULL,
    };

    char *response_data = NULL;
    int status_code = 0;
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        free(response_data);
        return err;
    }
    cJSON *response = cJSON_Parse(response_data);
    free(response_data);

    if (!response) {
        return ESP_FAIL;
    }

    matter_device_t *list_head = NULL;
    char *self_node_id = esp_rmaker_get_node_id();
    cJSON *groups = cJSON_GetObjectItem(response, "groups");
    if (groups && cJSON_IsArray(groups) && cJSON_GetArraySize(groups) > 0) {
        cJSON *group = cJSON_GetArrayItem(groups, 0);
        cJSON *node_details = cJSON_GetObjectItem(group, "node_details");
        if (node_details && cJSON_IsArray(node_details)) {
            int count = cJSON_GetArraySize(node_details);
            for (int i = 0; i < count; i++) {
                cJSON *node = cJSON_GetArrayItem(node_details, i);
                cJSON *id = cJSON_GetObjectItem(node, "id");
                cJSON *matter_id = cJSON_GetObjectItem(node, "matter_node_id");
                if (id && id->valuestring && strncmp(id->valuestring, self_node_id, strlen(self_node_id)) != 0 &&
                    matter_id && matter_id->valuestring) {
                    matter_device_t *dev = (matter_device_t *)MEM_CALLOC_EXTRAM(1, sizeof(matter_device_t));
                    if (dev) {
                        strncpy(dev->rainmaker_node_id, id->valuestring, ESP_RAINMAKER_NODE_ID_MAX_LEN - 1);
                        dev->rainmaker_node_id[ESP_RAINMAKER_NODE_ID_MAX_LEN - 1] = '\0';
                        dev->node_id = convert_hex_string_to_uint64(matter_id->valuestring);
                        dev->next = list_head;
                        list_head = dev;
                    }
                }
            }
        }
    }
    cJSON_Delete(response);
    *device_list = list_head;
    return ESP_OK;
}

static esp_err_t fetch_matter_node_metadata(matter_device_t *device)
{
    if (!device) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strnlen(device->rainmaker_node_id, sizeof(device->rainmaker_node_id)) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char query_parameters[RAINMAKER_URL_LEN] = {0};
    snprintf(query_parameters, sizeof(query_parameters),
             "node_id=%s&node_details=true&status=true&config=false&params=false&is_matter=true",
             device->rainmaker_node_id);

    app_rmaker_user_api_request_config_t request_config = {
#ifdef MATTER_CONTROLLER_REUSE_SESSION
        .reuse_session = true,
#else
        .reuse_session = false,
#endif
        .api_type = APP_RMAKER_USER_API_TYPE_GET,
        .api_name = "user/nodes",
        .api_version = NULL,
        .api_query_params = query_parameters,
        .api_payload = NULL,
    };

    char *response_data = NULL;
    int status_code = 0;
    esp_err_t err = app_rmaker_user_api_generic(&request_config, &status_code, &response_data);
    if (err != ESP_OK) {
        free(response_data);
        return err;
    }
    cJSON *response = cJSON_Parse(response_data);
    free(response_data);

    if (!response) {
        return ESP_FAIL;
    }

    err = ESP_FAIL;
    cJSON *nodes = cJSON_GetObjectItem(response, "node_details");
    if (nodes && cJSON_IsArray(nodes) && cJSON_GetArraySize(nodes) == 1) {
        cJSON *node = cJSON_GetArrayItem(nodes, 0);
        cJSON *status = cJSON_GetObjectItem(node, "status");
        if (status) {
            cJSON *connectivity = cJSON_GetObjectItem(status, "connectivity");
            if (connectivity) {
                cJSON *connected = cJSON_GetObjectItem(connectivity, "connected");
                if (connected && cJSON_IsBool(connected)) {
                    device->reachable = connected->valueint;
                }
            }
        }
        cJSON *metadata = cJSON_GetObjectItem(node, "metadata");
        if (metadata) {
            cJSON *matter = cJSON_GetObjectItem(metadata, "Matter");
            if (matter) {
                cJSON *device_type = cJSON_GetObjectItem(matter, "deviceType");
                if (device_type && cJSON_IsNumber(device_type)) {
                    device->endpoint_count = 1;
                    device->endpoints[0].device_type_id = (uint32_t)device_type->valueint;
                    device->endpoints[0].endpoint_id = 1; /* Default endpoint ID */
                    cJSON *endpoints_data = cJSON_GetObjectItem(matter, "endpoints");
                    if (endpoints_data && cJSON_IsArray(endpoints_data)) {
                        int ep_count = cJSON_GetArraySize(endpoints_data);
                        int ep_id = 1;
                        cJSON *ep_item = NULL;
                        if (ep_count > 1) {
                            ep_item = cJSON_GetArrayItem(endpoints_data, 1);
                        } else if (ep_count == 1) {
                            ep_item = cJSON_GetArrayItem(endpoints_data, 0);
                        }
                        if (ep_item && cJSON_IsNumber(ep_item)) {
                            ep_id = ep_item->valueint;
                        }
                        device->endpoints[0].endpoint_id = (uint16_t)ep_id;
                    }
                }

                cJSON *device_name = cJSON_GetObjectItem(matter, "deviceName");
                if (device_name && cJSON_IsString(device_name) && device_name->valuestring) {
                    strncpy(device->endpoints[0].device_name, device_name->valuestring,
                            ESP_MATTER_DEVICE_NAME_MAX_LEN - 1);
                    device->endpoints[0].device_name[ESP_MATTER_DEVICE_NAME_MAX_LEN - 1] = '\0';
                }

                cJSON *is_rainmaker = cJSON_GetObjectItem(matter, "isRainmaker");
                if (is_rainmaker && cJSON_IsBool(is_rainmaker)) {
                    device->is_rainmaker_device = is_rainmaker->valueint;
                }
            }
            device->is_metadata_fetched = true;
        }
        err = ESP_OK;
    }
    cJSON_Delete(response);
    return err;
}

// /v1/user/node_group - Get Matter device list
esp_err_t app_rmaker_api_get_matter_device_list(const char *group_id, matter_device_t **device_list)
{
    if (!group_id || !device_list) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }
    *device_list = NULL;

    ESP_RETURN_ON_ERROR(fetch_matter_node_list(group_id, device_list), TAG, "Failed to get matter node list");

    if (*device_list) {
        matter_device_t *dev = *device_list;
        while (dev) {
            esp_err_t err = fetch_matter_node_metadata(dev);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to fetch metadata for Matter Node 0x%08" PRIx32 "%08" PRIx32,
                         (uint32_t)(dev->node_id >> 32), (uint32_t)(dev->node_id & 0xFFFFFFFF));
                app_rmaker_free_matter_device_list(*device_list);
                *device_list = NULL;
                return err;
            }
            dev = dev->next;
        }
    }
    return ESP_OK;
}

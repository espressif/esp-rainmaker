/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include <json_parser.h>
#include <json_generator.h>
#include <esp_event.h>
#include "esp_rmaker_prov_local_ctrl.pb-c.h"
#include "esp_rmaker_internal.h"
#include <esp_rmaker_core.h>
#include <esp_rmaker_utils.h>
#include <network_provisioning/manager.h>

static const char *TAG = "esp_rmaker_prov_local_ctrl";

#define GET_PARAMS_ENDPOINT       "get_params"
#define SET_PARAMS_ENDPOINT       "set_params"
#define GET_CONFIG_ENDPOINT       "get_config"
#define DATA_FRAGMENT_SIZE        200

/* Data type for local control */
typedef enum {
    PROV_LOCAL_CTRL_TYPE_PARAMS = 0,
    PROV_LOCAL_CTRL_TYPE_CONFIG = 1,
} prov_local_ctrl_data_type_t;

/* Structure to hold data (params or config) during fragmented transfer */
typedef struct {
    char *data;
    size_t data_len;
    prov_local_ctrl_data_type_t data_type;
    bool has_timestamp;
    int64_t timestamp;
} prov_local_ctrl_data_t;

static prov_local_ctrl_data_t *s_local_ctrl_data = NULL;

/* Reassembly buffer for fragmented set_params. Separate from the get buffer. */
typedef struct {
    uint8_t *data;
    uint32_t total_len;
    uint32_t received;
} prov_set_frag_t;

static prov_set_frag_t *s_set_frag = NULL;

#ifdef CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_SET_FRAG
static esp_err_t esp_rmaker_set_params_frag_body(RmakerProvLocalCtrl__RMakerLocalCtrlPayload *msg,
                                                 uint8_t **outbuf, ssize_t *outlen);
#endif

/**
 * @brief Create a JSON error response
 *
 * Creates a response in format: {"status":"fail","description":"<reason>"}
 * Returns ESP_OK to keep BLE connection alive.
 *
 * @param[in] reason Error description string
 * @param[out] outbuf Pointer to store the allocated response buffer
 * @param[out] outlen Pointer to store the response length
 *
 * @return ESP_OK (always, to prevent BLE disconnection)
 */
static esp_err_t send_error_response(const char *reason, uint8_t **outbuf, ssize_t *outlen)
{
    char err_msg[128];
    snprintf(err_msg, sizeof(err_msg), "{\"status\":\"fail\",\"description\":\"%s\"}", reason ? reason : "unknown error");

    size_t resp_len = strlen(err_msg);
    uint8_t *resp_buf = (uint8_t *)MEM_ALLOC_EXTRAM(resp_len);
    if (!resp_buf) {
        /* Last resort - if we can't even allocate error message, return empty response */
        *outbuf = NULL;
        *outlen = 0;
        return ESP_OK;
    }

    memcpy(resp_buf, err_msg, resp_len);
    *outbuf = resp_buf;
    *outlen = resp_len;

    return ESP_OK;
}

static void prov_local_ctrl_data_free(void)
{
    if (s_local_ctrl_data) {
        if (s_local_ctrl_data->data) {
            free(s_local_ctrl_data->data);
        }
        free(s_local_ctrl_data);
        s_local_ctrl_data = NULL;
    }
}

static const char *prov_local_ctrl_data_name(prov_local_ctrl_data_type_t data_type)
{
    if (data_type == PROV_LOCAL_CTRL_TYPE_PARAMS) {
        return "params";
    }
    return "config";
}

static void prov_set_frag_free(void)
{
    if (s_set_frag) {
        free(s_set_frag->data);
        free(s_set_frag);
        s_set_frag = NULL;
    }
}

static esp_err_t create_signed_response(char *data, int64_t timestamp,
                                             char **out_response, size_t *out_len)
{
    if (!data || !out_response || !out_len) {
        if (data) {
            free(data);
        }
        return ESP_ERR_INVALID_ARG;
    }

    *out_response = NULL;
    *out_len = 0;

    size_t data_len = strlen(data);

    /* Step 1: Create node_payload JSON: {"data": <data>, "timestamp": <timestamp>} */
    size_t payload_buf_size = data_len + 100; /* Extra space for structure */
    char *payload_buf = (char *)MEM_ALLOC_EXTRAM(payload_buf_size);
    if (!payload_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for payload");
        free(data);
        return ESP_ERR_NO_MEM;
    }

    json_gen_str_t jstr;
    json_gen_str_start(&jstr, payload_buf, payload_buf_size, NULL, NULL);
    json_gen_start_object(&jstr);
    json_gen_push_object_str(&jstr, "data", data);
    json_gen_obj_set_int(&jstr, "timestamp", (int)timestamp);
    if (json_gen_end_object(&jstr) < 0) {
        ESP_LOGE(TAG, "Failed to generate payload JSON (buffer too small)");
        free(payload_buf);
        free(data);
        return ESP_ERR_NO_MEM;
    }
    json_gen_str_end(&jstr);
    size_t payload_strlen = strlen(payload_buf);

    /* Free input data early since json_generator has already copied it */
    free(data);

    ESP_LOGD(TAG, "Payload to sign (length %d): %s", payload_strlen, payload_buf);

    /* Step 2: Sign the payload */
    void *signature = NULL;
    size_t signature_len = 0;
    esp_err_t err = esp_rmaker_node_auth_sign_msg(payload_buf, payload_strlen, &signature, &signature_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sign payload: %s", esp_err_to_name(err));
        free(payload_buf);
        return err;
    }

    /* Step 3: Create final response: {"node_payload": <payload>, "signature": "..."} */
    size_t final_buf_size = payload_strlen + signature_len + 100;
    char *final_buf = (char *)MEM_ALLOC_EXTRAM(final_buf_size);
    if (!final_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for final response");
        free(signature);
        free(payload_buf);
        return ESP_ERR_NO_MEM;
    }

    json_gen_str_t final_jstr;
    json_gen_str_start(&final_jstr, final_buf, final_buf_size, NULL, NULL);
    json_gen_start_object(&final_jstr);
    json_gen_push_object_str(&final_jstr, "node_payload", payload_buf);
    json_gen_obj_set_string(&final_jstr, "signature", (char *)signature);
    if (json_gen_end_object(&final_jstr) < 0) {
        ESP_LOGE(TAG, "Failed to generate final JSON (buffer too small)");
        free(signature);
        free(payload_buf);
        free(final_buf);
        return ESP_ERR_NO_MEM;
    }
    json_gen_str_end(&final_jstr);

    /* Free intermediate buffers */
    free(signature);
    free(payload_buf);

    *out_response = final_buf;
    *out_len = strlen(final_buf);

    return ESP_OK;
}

#ifdef CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_GET_FILTER
static esp_err_t filter_copy_device_name(const jparse_ctx_t *jctx, json_tok_t *key, char **out)
{
    /* Read the key token as a standalone JSON string, with escapes decoded. */
    jparse_ctx_t name_ctx = { .js = jctx->js, .tokens = key };
    int nlen = 0;
    if (json_root_get_strlen(&name_ctx, &nlen) != 0 || nlen <= 0 ||
            nlen > ESP_RMAKER_PARAM_FILTER_MAX_NAME_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    char *name = MEM_CALLOC_EXTRAM(1, (size_t)nlen + 1);
    if (!name) {
        return ESP_ERR_NO_MEM;
    }
    if (json_root_get_string(&name_ctx, name, nlen + 1) != 0) {
        free(name);
        return ESP_ERR_INVALID_ARG;
    }
    *out = name;
    return ESP_OK;
}

static esp_err_t filter_copy_arr_string(jparse_ctx_t *jctx, uint32_t index, char **out)
{
    int nlen = 0;
    if (json_arr_get_strlen(jctx, index, &nlen) != 0 || nlen <= 0 ||
            nlen > ESP_RMAKER_PARAM_FILTER_MAX_NAME_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    char *name = MEM_CALLOC_EXTRAM(1, (size_t)nlen + 1);
    if (!name) {
        return ESP_ERR_NO_MEM;
    }
    if (json_arr_get_string(jctx, index, name, nlen + 1) != 0) {
        free(name);
        return ESP_ERR_INVALID_ARG;
    }
    *out = name;
    return ESP_OK;
}

static void param_filter_free(esp_rmaker_param_filter_t *filters, size_t filter_count)
{
    for (size_t i = 0; i < filter_count; i++) {
        free(filters[i].name);
        for (size_t p = 0; p < filters[i].param_count; p++) {
            free(filters[i].param_names[p]);
        }
        free(filters[i].param_names);
    }
    free(filters);
}

static esp_err_t param_filter_parse(const uint8_t *json, size_t json_len,
                                    esp_rmaker_param_filter_t **filters, size_t *filter_count)
{
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (const char *)json, (int)json_len) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    json_tok_t *params = json_obj_get_val_tok(&jctx, "params", JSMN_OBJECT);
    if (!params || params->size <= 0 || params->size > ESP_RMAKER_PARAM_FILTER_MAX_DEVICES) {
        json_parse_end(&jctx);
        return ESP_ERR_INVALID_ARG;
    }

    esp_rmaker_param_filter_t *parsed = MEM_CALLOC_EXTRAM((size_t)params->size, sizeof(*parsed));
    if (!parsed) {
        json_parse_end(&jctx);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;
    json_tok_t *key = params + 1;
    for (int i = 0; i < params->size; i++) {
        err = filter_copy_device_name(&jctx, key, &parsed[i].name);
        if (err != ESP_OK) {
            break;
        }
        json_tok_t *value = key + 1;
        if (value->type == JSMN_PRIMITIVE) {
            jparse_ctx_t value_ctx = { .js = jctx.js, .tokens = value };
            bool all_params = false;
            if (json_root_get_bool(&value_ctx, &all_params) != 0 || !all_params) {
                err = ESP_ERR_INVALID_ARG;
                break;
            }
            key = value + 1;
            continue;
        }
        if (value->type != JSMN_ARRAY || value->size <= 0 ||
                value->size > ESP_RMAKER_PARAM_FILTER_MAX_PARAMS) {
            err = ESP_ERR_INVALID_ARG;
            break;
        }
        parsed[i].param_names = MEM_CALLOC_EXTRAM((size_t)value->size, sizeof(char *));
        if (!parsed[i].param_names) {
            err = ESP_ERR_NO_MEM;
            break;
        }
        parsed[i].param_count = (size_t)value->size;
        jctx.cur = value;
        for (int p = 0; p < value->size; p++) {
            err = filter_copy_arr_string(&jctx, (uint32_t)p, &parsed[i].param_names[p]);
            if (err != ESP_OK) {
                break;
            }
        }
        if (err != ESP_OK) {
            break;
        }
        /* Each accepted array element is a single string token. */
        key = value + 1 + value->size;
    }
    size_t num = (size_t)params->size;
    json_parse_end(&jctx);

    if (err != ESP_OK) {
        param_filter_free(parsed, num);
        return err;
    }
    *filters = parsed;
    *filter_count = num;
    return ESP_OK;
}
#endif /* CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_GET_FILTER */


static esp_err_t prov_local_ctrl_data_init(prov_local_ctrl_data_type_t data_type, bool has_timestamp, int64_t timestamp,
                                           const uint8_t *params_filter, size_t params_filter_len)
{
    /* Free any existing data */
    prov_local_ctrl_data_free();

    /* Allocate data structure */
    s_local_ctrl_data = MEM_CALLOC_EXTRAM(1, sizeof(prov_local_ctrl_data_t));
    if (!s_local_ctrl_data) {
        ESP_LOGE(TAG, "Failed to allocate local ctrl data structure");
        return ESP_ERR_NO_MEM;
    }

    s_local_ctrl_data->data_type = data_type;
    s_local_ctrl_data->has_timestamp = has_timestamp;
    s_local_ctrl_data->timestamp = timestamp;

    char *raw_data = NULL;
    const char *data_name = prov_local_ctrl_data_name(data_type);

    /* Get the raw data based on type */
    if (data_type == PROV_LOCAL_CTRL_TYPE_PARAMS) {
#ifdef CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_GET_FILTER
        if (params_filter_len > 0) {
            esp_rmaker_param_filter_t *filters = NULL;
            size_t filter_count = 0;
            esp_err_t err = param_filter_parse(params_filter, params_filter_len, &filters, &filter_count);
            if (err != ESP_OK) {
                prov_local_ctrl_data_free();
                return err;
            }
            err = esp_rmaker_get_filtered_node_params(filters, filter_count, &raw_data);
            param_filter_free(filters, filter_count);
            if (err != ESP_OK) {
                prov_local_ctrl_data_free();
                return err;
            }
        } else
#endif
        {
            raw_data = esp_rmaker_get_node_params();
        }
    } else {
        raw_data = esp_rmaker_get_node_config();
    }

    if (!raw_data) {
        ESP_LOGE(TAG, "Failed to get node %s", data_name);
        prov_local_ctrl_data_free();
        return ESP_ERR_NO_MEM;
    }

    if (has_timestamp) {
        /* Create signed response using helper function.
         * raw_data ownership is transferred to create_signed_response. */
        char *signed_data = NULL;
        size_t signed_data_len = 0;
        esp_err_t err = create_signed_response(raw_data, timestamp, &signed_data, &signed_data_len);
        if (err != ESP_OK) {
            prov_local_ctrl_data_free();
            return err;
        }

        /* Store the signed data */
        s_local_ctrl_data->data = signed_data;
        s_local_ctrl_data->data_len = signed_data_len;

        ESP_LOGI(TAG, "Get %s response (signed, len=%d): %.*s",
                 data_name, s_local_ctrl_data->data_len, s_local_ctrl_data->data_len, s_local_ctrl_data->data);
    } else {
        /* No timestamp - just store the raw data */
        s_local_ctrl_data->data = raw_data;
        s_local_ctrl_data->data_len = strlen(raw_data);

        ESP_LOGI(TAG, "Get %s response (raw, len=%d): %.*s",
                 data_name, s_local_ctrl_data->data_len, s_local_ctrl_data->data_len, s_local_ctrl_data->data);
    }

    return ESP_OK;
}

/**
 * @brief Send one fragment of a get response.
 *
 * params_filter is used only for filtered params when requested_offset is 0.
 * reject skips generation and returns reject_status with an empty buffer.
 */
static esp_err_t esp_rmaker_reply_get_data(uint8_t **outbuf, ssize_t *outlen,
                                           prov_local_ctrl_data_type_t data_type,
                                           bool reject,
                                           RmakerProvLocalCtrl__RMakerLocalCtrlStatus reject_status,
                                           uint32_t requested_offset, bool has_timestamp, int64_t timestamp,
                                           const uint8_t *params_filter, size_t params_filter_len)
{
    if (!outbuf || !outlen) {
        ESP_LOGE(TAG, "Invalid params");
        return ESP_ERR_INVALID_ARG;
    }

    const char *data_name = prov_local_ctrl_data_name(data_type);

    /* Initialize response */
    RmakerProvLocalCtrl__RMakerLocalCtrlPayload response = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_PAYLOAD__INIT;
    response.msg = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_MSG_TYPE__TypeRespGetData;

    RmakerProvLocalCtrl__RespGetData resp_get_data = RMAKER_PROV_LOCAL_CTRL__RESP_GET_DATA__INIT;
    RmakerProvLocalCtrl__PayloadBuf payload_buf = RMAKER_PROV_LOCAL_CTRL__PAYLOAD_BUF__INIT;

    resp_get_data.buf = &payload_buf;
    response.payload_case = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_PAYLOAD__PAYLOAD_RESP_GET_DATA;
    response.respgetdata = &resp_get_data;

    bool is_last_fragment = false;
    bool is_first_fragment = false;

    if (reject) {
        resp_get_data.status = reject_status;
        goto send_response;
    }

    is_first_fragment = (requested_offset == 0);

    /* Log only for first fragment, use LOGD for others */
    if (is_first_fragment) {
        ESP_LOGI(TAG, "Get %s handler: offset=%lu, has_timestamp=%d, timestamp=%lld",
                 data_name, (unsigned long)requested_offset, has_timestamp, timestamp);
    } else {
        ESP_LOGD(TAG, "Get %s handler: offset=%lu", data_name, (unsigned long)requested_offset);
    }

    /* If offset is 0, initialize/reinitialize the data */
    if (is_first_fragment) {
        esp_err_t err = prov_local_ctrl_data_init(data_type, has_timestamp, timestamp, params_filter, params_filter_len);
        if (err != ESP_OK) {
            if (err == ESP_ERR_INVALID_ARG) {
                resp_get_data.status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__InvalidParam;
            } else {
                resp_get_data.status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__NoMemory;
            }
            goto send_response;
        }
    }

    /* Check if this continuation belongs to the transfer started at offset 0. */
    if (!s_local_ctrl_data || !s_local_ctrl_data->data || s_local_ctrl_data->data_type != data_type) {
        ESP_LOGE(TAG, "%s data not initialized", data_name);
        resp_get_data.status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__Fail;
        goto send_response;
    }

    /* Validate requested offset */
    if (requested_offset > s_local_ctrl_data->data_len) {
        ESP_LOGE(TAG, "Requested offset %lu exceeds %s length %d",
                 (unsigned long)requested_offset, data_name, s_local_ctrl_data->data_len);
        resp_get_data.status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__InvalidParam;
        goto send_response;
    }

    /* Calculate fragment size */
    size_t remaining = s_local_ctrl_data->data_len - requested_offset;
    size_t fragment_size = (remaining > DATA_FRAGMENT_SIZE) ? DATA_FRAGMENT_SIZE : remaining;

    /* Set up payload buffer */
    payload_buf.offset = requested_offset;
    payload_buf.totallen = s_local_ctrl_data->data_len;
    payload_buf.payload.data = (uint8_t *)(s_local_ctrl_data->data + requested_offset);
    payload_buf.payload.len = fragment_size;

    resp_get_data.status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__Success;

    /* Track if this is the last fragment */
    is_last_fragment = (requested_offset + fragment_size >= s_local_ctrl_data->data_len);

    /* Log fragment info: LOGI for first/last, LOGD for middle */
    if (is_first_fragment) {
        ESP_LOGI(TAG, "Sending first %s fragment: len=%d, total=%d",
                 data_name, fragment_size, s_local_ctrl_data->data_len);
    } else if (is_last_fragment) {
        ESP_LOGI(TAG, "Sending last %s fragment: offset=%lu, len=%d, total=%d",
                 data_name, (unsigned long)requested_offset, fragment_size, s_local_ctrl_data->data_len);
    } else {
        ESP_LOGD(TAG, "Sending %s fragment: offset=%lu, len=%d, total=%d",
                 data_name, (unsigned long)requested_offset, fragment_size, s_local_ctrl_data->data_len);
    }

send_response:
    /* Serialize the response */
    size_t resp_len = rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__get_packed_size(&response);
    uint8_t *resp_buf = MEM_ALLOC_EXTRAM(resp_len);
    if (!resp_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return ESP_ERR_NO_MEM;
    }

    rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__pack(&response, resp_buf);
    *outbuf = resp_buf;
    *outlen = resp_len;

    /* Free data AFTER serialization if this was the last fragment */
    if (is_last_fragment) {
        prov_local_ctrl_data_free();
        ESP_LOGI(TAG, "Get %s completed, data freed", data_name);
    }

    return ESP_OK;
}

static esp_err_t esp_rmaker_get_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                              uint8_t **outbuf, ssize_t *outlen, void *priv_data,
                                              prov_local_ctrl_data_type_t data_type)
{
    (void)session_id;
    (void)priv_data;
    if (!outbuf || !outlen) {
        ESP_LOGE(TAG, "Invalid params");
        return ESP_ERR_INVALID_ARG;
    }

    RmakerProvLocalCtrl__RMakerLocalCtrlPayload *msg = NULL;
    if (inbuf && inlen > 0) {
        msg = rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__unpack(NULL, (size_t)inlen, inbuf);
        if (!msg) {
            ESP_LOGE(TAG, "Failed to unpack message");
            return ESP_ERR_INVALID_ARG;
        }
    }

    bool reject = false;
    uint32_t requested_offset = 0;
    bool has_timestamp = false;
    int64_t timestamp = 0;
    if (msg && msg->payload_case == RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_PAYLOAD__PAYLOAD_CMD_GET_DATA &&
            msg->cmdgetdata) {
        RmakerProvLocalCtrl__RMakerLocalCtrlDataType expect =
            (data_type == PROV_LOCAL_CTRL_TYPE_PARAMS) ?
            RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_DATA_TYPE__TypeParams :
            RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_DATA_TYPE__TypeConfig;
        if (msg->cmdgetdata->datatype != expect) {
            ESP_LOGE(TAG, "Data type mismatch in request");
            reject = true;
        } else {
            requested_offset = msg->cmdgetdata->offset;
            has_timestamp = msg->cmdgetdata->hastimestamp;
            timestamp = msg->cmdgetdata->timestamp;
        }
    }

    esp_err_t err = esp_rmaker_reply_get_data(outbuf, outlen, data_type, reject,
                                              RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__InvalidParam,
                                              requested_offset, has_timestamp, timestamp, NULL, 0);
    if (msg) {
        rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__free_unpacked(msg, NULL);
    }
    return err;
}

static esp_err_t esp_rmaker_get_params_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                                uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    (void)session_id;
    (void)priv_data;
    if (!outbuf || !outlen) {
        ESP_LOGE(TAG, "Invalid params");
        return ESP_ERR_INVALID_ARG;
    }

    RmakerProvLocalCtrl__RMakerLocalCtrlPayload *msg = NULL;
    if (inbuf && inlen > 0) {
        msg = rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__unpack(NULL, (size_t)inlen, inbuf);
        if (!msg) {
            ESP_LOGE(TAG, "Failed to unpack get_params message");
            return ESP_ERR_INVALID_ARG;
        }
    }

    bool reject = false;
    uint32_t requested_offset = 0;
    bool has_timestamp = false;
    int64_t timestamp = 0;
    const uint8_t *params_filter = NULL;
    size_t params_filter_len = 0;
    if (msg && msg->payload_case == RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_PAYLOAD__PAYLOAD_CMD_GET_DATA &&
            msg->cmdgetdata) {
        if (msg->cmdgetdata->datatype != RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_DATA_TYPE__TypeParams) {
            ESP_LOGE(TAG, "Data type mismatch in get_params request");
            reject = true;
        } else {
            requested_offset = msg->cmdgetdata->offset;
            has_timestamp = msg->cmdgetdata->hastimestamp;
            timestamp = msg->cmdgetdata->timestamp;
            if (requested_offset == 0 && msg->cmdgetdata->paramsfilter.len > 0) {
#ifdef CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_GET_FILTER
                params_filter = msg->cmdgetdata->paramsfilter.data;
                params_filter_len = msg->cmdgetdata->paramsfilter.len;
                if (params_filter_len > ESP_RMAKER_PARAM_FILTER_MAX_JSON_LEN) {
                    ESP_LOGE(TAG, "ParamsFilter too long (%u)", (unsigned)params_filter_len);
                    reject = true;
                }
#else
                ESP_LOGW(TAG, "ParamsFilter ignored; GET_FILTER disabled");
#endif
            }
        }
    }

    esp_err_t err = esp_rmaker_reply_get_data(outbuf, outlen, PROV_LOCAL_CTRL_TYPE_PARAMS, reject,
                                              RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__InvalidParam,
                                              requested_offset, has_timestamp, timestamp,
                                              params_filter, params_filter_len);
    if (msg) {
        rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__free_unpacked(msg, NULL);
    }
    return err;
}

static esp_err_t esp_rmaker_set_params_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                                uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    (void)session_id;
    (void)priv_data;
    ESP_LOGI(TAG, "Set params handler invoked");
    if (!inbuf || inlen <= 0 || !outbuf || !outlen) {
        ESP_LOGE(TAG, "Invalid params");
        if (outbuf && outlen) {
            return send_error_response("invalid request", outbuf, outlen);
        }
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t *body = inbuf;
    ssize_t body_len = inlen;
    while (body_len > 0 && isspace((unsigned char)*body)) {
        body++;
        body_len--;
    }

#ifdef CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_SET_FRAG
    if (body_len <= 0 || body[0] != '{') {
        RmakerProvLocalCtrl__RMakerLocalCtrlPayload *msg =
            rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__unpack(NULL, (size_t)inlen, inbuf);
        if (!msg) {
            ESP_LOGE(TAG, "Failed to unpack set_params fragment request");
            return send_error_response("invalid request", outbuf, outlen);
        }
        esp_err_t err = esp_rmaker_set_params_frag_body(msg, outbuf, outlen);
        rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__free_unpacked(msg, NULL);
        return err;
    }
#else
    if (body_len <= 0 || body[0] != '{') {
        return send_error_response("invalid request", outbuf, outlen);
    }
#endif

    ESP_LOGD(TAG, "Received params data of length: %d", (int)inlen);
    esp_err_t err = esp_rmaker_handle_set_params((char *)inbuf, (size_t)inlen, ESP_RMAKER_REQ_SRC_BLE_LOCAL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to handle set params: %s", esp_err_to_name(err));
        return send_error_response("failed to set params", outbuf, outlen);
    }

    const char *success_msg = "{\"status\":\"success\"}";
    size_t resp_len = strlen(success_msg);
    uint8_t *resp_buf = (uint8_t *)MEM_ALLOC_EXTRAM(resp_len);
    if (!resp_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        return send_error_response("no memory", outbuf, outlen);
    }

    memcpy(resp_buf, success_msg, resp_len);
    *outbuf = resp_buf;
    *outlen = (ssize_t)resp_len;

    ESP_LOGI(TAG, "Set params handler completed successfully");
    return ESP_OK;
}

static esp_err_t esp_rmaker_get_config_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                                uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    return esp_rmaker_get_data_handler(session_id, inbuf, inlen, outbuf, outlen, priv_data,
                                       PROV_LOCAL_CTRL_TYPE_CONFIG);
}

#ifdef CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_SET_FRAG
static esp_err_t prov_set_frag_send(RmakerProvLocalCtrl__RMakerLocalCtrlStatus status,
                                    uint32_t received, uint8_t **outbuf, ssize_t *outlen)
{
    RmakerProvLocalCtrl__RMakerLocalCtrlPayload response =
        RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_PAYLOAD__INIT;
    response.msg = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_MSG_TYPE__TypeRespSetData;

    RmakerProvLocalCtrl__RespSetData resp = RMAKER_PROV_LOCAL_CTRL__RESP_SET_DATA__INIT;
    resp.status = status;
    resp.received = received;
    resp.maxlen = CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_SET_FRAG_MAX_LEN;
    response.payload_case = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_PAYLOAD__PAYLOAD_RESP_SET_DATA;
    response.respsetdata = &resp;

    size_t resp_len = rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__get_packed_size(&response);
    uint8_t *resp_buf = MEM_ALLOC_EXTRAM(resp_len);
    if (!resp_buf) {
        ESP_LOGE(TAG, "Failed to allocate set_params fragment response");
        return ESP_ERR_NO_MEM;
    }
    rmaker_prov_local_ctrl__rmaker_local_ctrl_payload__pack(&response, resp_buf);
    *outbuf = resp_buf;
    *outlen = (ssize_t)resp_len;
    return ESP_OK;
}
static esp_err_t esp_rmaker_set_params_frag_body(RmakerProvLocalCtrl__RMakerLocalCtrlPayload *msg,
                                                 uint8_t **outbuf, ssize_t *outlen)
{
    esp_err_t send_err = ESP_OK;
    RmakerProvLocalCtrl__RMakerLocalCtrlStatus status =
        RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__InvalidParam;
    uint32_t received_out = 0;
    const uint32_t set_frag_max_total = CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_SET_FRAG_MAX_LEN;

    if (msg->payload_case != RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_PAYLOAD__PAYLOAD_CMD_SET_DATA ||
            !msg->cmdsetdata || !msg->cmdsetdata->buf) {
        ESP_LOGE(TAG, "set_params fragment request is not CmdSetData");
        prov_set_frag_free();
        goto respond;
    }

    RmakerProvLocalCtrl__PayloadBuf *frag = msg->cmdsetdata->buf;
    uint32_t offset = frag->offset;
    uint32_t total = frag->totallen;
    size_t len = frag->payload.len;
    const uint8_t *bytes = frag->payload.data;

    if (len == 0 || len > DATA_FRAGMENT_SIZE || total == 0 || total > set_frag_max_total ||
            offset > total || len > (size_t)(total - offset) || !bytes) {
        ESP_LOGE(TAG, "Invalid set_params fragment offset=%lu len=%u total=%lu",
                 (unsigned long)offset, (unsigned)len, (unsigned long)total);
        prov_set_frag_free();
        goto respond;
    }

    if (offset == 0) {
        prov_set_frag_free();
        s_set_frag = MEM_CALLOC_EXTRAM(1, sizeof(*s_set_frag));
        if (!s_set_frag) {
            status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__NoMemory;
            goto respond;
        }
        s_set_frag->data = MEM_CALLOC_EXTRAM(1, (size_t)total + 1);
        if (!s_set_frag->data) {
            prov_set_frag_free();
            status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__NoMemory;
            goto respond;
        }
        s_set_frag->total_len = total;
        s_set_frag->received = 0;
    } else if (!s_set_frag || s_set_frag->total_len != total || offset != s_set_frag->received) {
        ESP_LOGE(TAG, "set_params fragment is not contiguous");
        prov_set_frag_free();
        goto respond;
    }

    memcpy(s_set_frag->data + offset, bytes, len);
    s_set_frag->received = offset + (uint32_t)len;

    if (s_set_frag->received == s_set_frag->total_len) {
        uint32_t total_len = s_set_frag->total_len;
        s_set_frag->data[total_len] = '\0';
        esp_err_t err = esp_rmaker_handle_set_params((char *)s_set_frag->data, total_len,
                                                     ESP_RMAKER_REQ_SRC_BLE_LOCAL);
        prov_set_frag_free();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to apply fragmented set params");
            status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__Fail;
            received_out = 0;
        } else {
            status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__Success;
            received_out = total_len;
            ESP_LOGI(TAG, "set_params fragment applied %lu bytes", (unsigned long)total_len);
        }
    } else {
        status = RMAKER_PROV_LOCAL_CTRL__RMAKER_LOCAL_CTRL_STATUS__Success;
        received_out = s_set_frag->received;
        ESP_LOGD(TAG, "set_params fragment stored %lu/%lu",
                 (unsigned long)received_out, (unsigned long)s_set_frag->total_len);
    }

respond:
    send_err = prov_set_frag_send(status, received_out, outbuf, outlen);
    return send_err;
}
#endif /* CONFIG_ESP_RMAKER_PROV_LOCAL_CTRL_SET_FRAG */

static esp_err_t esp_rmaker_get_params_endpoint_create(void)
{
    return network_prov_mgr_endpoint_create(GET_PARAMS_ENDPOINT);
}

static esp_err_t esp_rmaker_get_params_endpoint_register(void)
{
    return network_prov_mgr_endpoint_register(GET_PARAMS_ENDPOINT, esp_rmaker_get_params_handler, NULL);
}

static esp_err_t esp_rmaker_set_params_endpoint_create(void)
{
    return network_prov_mgr_endpoint_create(SET_PARAMS_ENDPOINT);
}

static esp_err_t esp_rmaker_set_params_endpoint_register(void)
{
    return network_prov_mgr_endpoint_register(SET_PARAMS_ENDPOINT, esp_rmaker_set_params_handler, NULL);
}

static esp_err_t esp_rmaker_get_config_endpoint_create(void)
{
    return network_prov_mgr_endpoint_create(GET_CONFIG_ENDPOINT);
}

static esp_err_t esp_rmaker_get_config_endpoint_register(void)
{
    return network_prov_mgr_endpoint_register(GET_CONFIG_ENDPOINT, esp_rmaker_get_config_handler, NULL);
}

static void esp_rmaker_prov_local_ctrl_event_handler(void *arg, esp_event_base_t event_base,
                                                  int32_t event_id, void *event_data)
{
    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
            case NETWORK_PROV_INIT: {
                /* Note: Capabilities are set by challenge-response code since prov_local_ctrl
                 * depends on challenge-response being enabled */
                if (esp_rmaker_get_params_endpoint_create() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to create get params endpoint.");
                }
                if (esp_rmaker_set_params_endpoint_create() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to create set params endpoint.");
                }
                if (esp_rmaker_get_config_endpoint_create() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to create get config endpoint.");
                }
                ESP_LOGI(TAG, "Prov local ctrl endpoints created successfully");
                break;
            }
            case NETWORK_PROV_START: {
                if (esp_rmaker_get_params_endpoint_register() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to register get params endpoint.");
                }
                if (esp_rmaker_set_params_endpoint_register() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to register set params endpoint.");
                }
                if (esp_rmaker_get_config_endpoint_register() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to register get config endpoint.");
                }
                ESP_LOGI(TAG, "Prov local ctrl endpoints registered successfully");
                break;
            }
            case NETWORK_PROV_END: {
                /* Clean up data, keep handler registered for future provisioning */
                prov_local_ctrl_data_free();
                prov_set_frag_free();
                ESP_LOGI(TAG, "Local ctrl data cleaned up");
                break;
            }
            case NETWORK_PROV_DEINIT: {
                /* Full cleanup since provisioning manager is being torn down */
                esp_rmaker_prov_local_ctrl_deinit();
                ESP_LOGI(TAG, "Prov local ctrl deinitialized");
                break;
            }
            default:
                break;
        }
    }
}

esp_err_t esp_rmaker_prov_local_ctrl_init(void)
{
    /* Register for all Wi-Fi Provisioning events */
    return esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
                                      &esp_rmaker_prov_local_ctrl_event_handler, NULL);
}

esp_err_t esp_rmaker_prov_local_ctrl_deinit(void)
{
    /* Unregister the event handler */
    esp_event_handler_unregister(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID,
                                 &esp_rmaker_prov_local_ctrl_event_handler);

    /* Clean up any remaining data */
    prov_local_ctrl_data_free();
    prov_set_frag_free();

    return ESP_OK;
}

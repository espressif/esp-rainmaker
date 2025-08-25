/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_log.h>
#include <esp_event.h>
#include "esp_rmaker_chal_resp.pb-c.h"
#include "esp_rmaker_internal.h"
#include <esp_rmaker_core.h>
#include <network_provisioning/manager.h>

static const char *TAG = "esp_rmaker_chal_resp";

#define CHAL_RESP_ENDPOINT       "ch_resp"
#define RMAKER_EXTRA_APP_NAME    "rmaker_extra"
#define RMAKER_EXTRA_APP_VERSION "1.0"

static esp_err_t esp_rmaker_handle_challenge(const char *challenge, size_t challenge_len, char **response, size_t *response_len)
{
    if (!challenge || !response || !response_len) {
        ESP_LOGE(TAG, "Invalid params");
        return ESP_ERR_INVALID_ARG;
    }

    /* Sign the challenge using node auth */
    esp_err_t err = esp_rmaker_node_auth_sign_msg((const void*)challenge, challenge_len, (void **)response, response_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to sign challenge");
        return err;
    }
    return ESP_OK;
}

static esp_err_t esp_rmaker_chal_resp_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen,
                                            uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    ESP_LOGI(TAG, "Challenge-Response handler invoked");
    if (!inbuf || !inlen || !outbuf || !outlen) {
        ESP_LOGE(TAG, "Invalid params");
        return ESP_ERR_INVALID_ARG;
    }
    ESP_LOGD(TAG, "Received challenge data of length: %d", inlen);

    /* Initialize all resources to NULL for safe cleanup */
    RmakerChResp__RMakerChRespPayload *msg = NULL;
    char *signed_data = NULL;
    uint8_t *binary_data = NULL;
    RmakerChResp__RespCRPayload *resp_payload = NULL;
    uint8_t *resp_buf = NULL;
    esp_err_t ret = ESP_FAIL;

    /* Parse the received protobuf message */
    msg = rmaker_ch_resp__rmaker_ch_resp_payload__unpack(NULL, inlen, inbuf);
    if (!msg) {
        ESP_LOGE(TAG, "Failed to unpack message");
        goto cleanup;
    }
    ESP_LOGD(TAG, "Successfully unpacked protobuf message");

    if (msg->msg != RMAKER_CH_RESP__RMAKER_CH_RESP_MSG_TYPE__TypeCmdChallengeResponse) {
        ESP_LOGE(TAG, "Invalid message type received: %d", msg->msg);
        goto cleanup;
    }
    ESP_LOGD(TAG, "Received valid challenge response command");

    /* Get the challenge from the message */
    RmakerChResp__CmdCRPayload *cmd_payload = msg->cmdchallengeresponsepayload;
    if (!cmd_payload || !cmd_payload->payload.data || !cmd_payload->payload.len) {
        ESP_LOGE(TAG, "Invalid challenge received");
        goto cleanup;
    }
    ESP_LOGI(TAG, "Challenge payload length: %d", cmd_payload->payload.len);
    ESP_LOGD(TAG, "Challenge string (len %d): %.*s", cmd_payload->payload.len, cmd_payload->payload.len, (char *)cmd_payload->payload.data);

    /* Sign the challenge */
    size_t signed_len = 0;
    esp_err_t err = esp_rmaker_handle_challenge((const char*)cmd_payload->payload.data,
                                              cmd_payload->payload.len,
                                              &signed_data, &signed_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to handle challenge");
        ret = err;
        goto cleanup;
    }
    ESP_LOGD(TAG, "Successfully signed challenge. Signature length: %d", signed_len);
    ESP_LOGD(TAG, "Original signature (hex, len %d): %.*s", signed_len, signed_len, (char *)signed_data);

    /* Convert hex string to binary */
    binary_data = calloc(1, signed_len/2);
    if (!binary_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for binary data");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    /* Convert each pair of hex chars to a byte */
    for (int i = 0; i < signed_len; i += 2) {
        char hex[3] = {((char *)signed_data)[i], ((char *)signed_data)[i+1], 0};
        uint8_t byte;
        sscanf(hex, "%02hhx", &byte);
        binary_data[i/2] = byte;
    }
    size_t binary_len = signed_len/2;

    /* Create response protobuf message */
    RmakerChResp__RMakerChRespPayload resp_msg = RMAKER_CH_RESP__RMAKER_CH_RESP_PAYLOAD__INIT;
    resp_msg.msg = RMAKER_CH_RESP__RMAKER_CH_RESP_MSG_TYPE__TypeRespChallengeResponse;
    resp_msg.status = RMAKER_CH_RESP__RMAKER_CH_RESP_STATUS__Success;

    /* Allocate the response payload structure */
    resp_payload = malloc(sizeof(RmakerChResp__RespCRPayload));
    if (!resp_payload) {
        ESP_LOGE(TAG, "Failed to allocate memory for response payload");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    rmaker_ch_resp__resp_crpayload__init(resp_payload);

    /* Set up the payload fields with binary data */
    resp_payload->payload.data = binary_data;
    resp_payload->payload.len = binary_len;
    resp_payload->node_id = strdup(esp_rmaker_get_node_id());
    if (!resp_payload->node_id) {
        ESP_LOGE(TAG, "Failed to allocate memory for node_id");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    ESP_LOGI(TAG, "Setting up response with node_id: %s, binary signature len: %d", resp_payload->node_id, resp_payload->payload.len);

    /* Set the oneof field */
    resp_msg.payload_case = RMAKER_CH_RESP__RMAKER_CH_RESP_PAYLOAD__PAYLOAD_RESP_CHALLENGE_RESPONSE_PAYLOAD;
    resp_msg.respchallengeresponsepayload = resp_payload;

    /* Verify response structure before packing */
    if (!resp_msg.respchallengeresponsepayload ||
        !resp_msg.respchallengeresponsepayload->payload.data ||
        !resp_msg.respchallengeresponsepayload->payload.len) {
        ESP_LOGE(TAG, "Response signature data not properly set");
        goto cleanup;
    }

    /* Serialize the response */
    size_t resp_len = rmaker_ch_resp__rmaker_ch_resp_payload__get_packed_size(&resp_msg);
    ESP_LOGD(TAG, "Calculated packed size: %d", resp_len);

    resp_buf = calloc(1, resp_len);
    if (!resp_buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    size_t packed_size = rmaker_ch_resp__rmaker_ch_resp_payload__pack(&resp_msg, resp_buf);
    ESP_LOGD(TAG, "Actually packed size: %d", packed_size);
    if (packed_size != resp_len) {
        ESP_LOGE(TAG, "Packed size mismatch! Expected: %d, Got: %d", resp_len, packed_size);
    }

    /* Success - transfer ownership of resp_buf to caller */
    *outbuf = resp_buf;
    *outlen = resp_len;
    resp_buf = NULL; /* Don't free this in cleanup */
    ret = ESP_OK;
    ESP_LOGI(TAG, "Challenge-Response handler completed successfully");

cleanup:
    /* Common cleanup section - safe to call with NULL pointers */
    if (msg) {
        rmaker_ch_resp__rmaker_ch_resp_payload__free_unpacked(msg, NULL);
    }
    if (resp_payload) {
        if (resp_payload->node_id) {
            free(resp_payload->node_id);
        }
        free(resp_payload);
    }
    if (binary_data) {
        free(binary_data);
    }
    if (signed_data) {
        free(signed_data);
    }
    if (resp_buf) {
        free(resp_buf);
    }

    return ret;
}

esp_err_t esp_rmaker_chal_resp_endpoint_create(void)
{
    return network_prov_mgr_endpoint_create(CHAL_RESP_ENDPOINT);
}

esp_err_t esp_rmaker_chal_resp_endpoint_register(void)
{
    return network_prov_mgr_endpoint_register(CHAL_RESP_ENDPOINT, esp_rmaker_chal_resp_handler, NULL);
}

static void esp_rmaker_chal_resp_event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    static const char *capabilities[] = {"ch_resp"};
    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
            case NETWORK_PROV_INIT: {
                network_prov_mgr_set_app_info(RMAKER_EXTRA_APP_NAME, RMAKER_EXTRA_APP_VERSION, capabilities, 1);
                if (esp_rmaker_chal_resp_endpoint_create() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to create challenge response endpoint.");
                }
                break;
            }
            case NETWORK_PROV_START: {
                if (esp_rmaker_chal_resp_endpoint_register() != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to register challenge response endpoint.");
                }
                break;
            }
            default:
                break;
        }
    }
}

esp_err_t esp_rmaker_chal_resp_init(void)
{
    /* Register for Wi-Fi Provisioning events */
    esp_err_t err = esp_event_handler_register(NETWORK_PROV_EVENT, NETWORK_PROV_INIT, &esp_rmaker_chal_resp_event_handler, NULL);
    if (err != ESP_OK) {
        return err;
    }
    err = esp_event_handler_register(NETWORK_PROV_EVENT, NETWORK_PROV_START, &esp_rmaker_chal_resp_event_handler, NULL);
    return err;
}

esp_err_t esp_rmaker_chal_resp_deinit(void)
{
    /* Unregister the event handler */
    esp_event_handler_unregister(NETWORK_PROV_EVENT, NETWORK_PROV_INIT, &esp_rmaker_chal_resp_event_handler);
    esp_event_handler_unregister(NETWORK_PROV_EVENT, NETWORK_PROV_START, &esp_rmaker_chal_resp_event_handler);
    return ESP_OK;
}

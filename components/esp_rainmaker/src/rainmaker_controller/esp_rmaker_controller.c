/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_mqtt_topics.h>
#include <esp_rmaker_mqtt.h>
#include <esp_rmaker_standard_services.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_controller.h>

#define ESP_RMAKER_DEF_CONTROLLER_SERVICE              "RMCTLService"

static const char *TAG = "RMController";
static esp_rmaker_device_t *controller_service = NULL;
static esp_rmaker_controller_config_t controller_config = {0};

static char *esp_rmaker_controller_parse_node_id(const char *topic)
{
    const char *prefix = "node/";
    size_t prefix_len = strlen(prefix);
    if (!topic || strncmp(topic, prefix, prefix_len) != 0) {
        ESP_LOGE(TAG, "Unsupported topic format: %s", topic ? topic : "NULL");
        return NULL;
    }

    const char *node_id_start = topic + prefix_len;
    const char *node_id_end = strchr(node_id_start, '/');
    if (!node_id_end || node_id_end == node_id_start) {
        ESP_LOGE(TAG, "Failed to locate node_id in topic: %s", topic);
        return NULL;
    }

    size_t node_id_len = node_id_end - node_id_start;
    char *node_id = calloc(1, node_id_len + 1);
    if (!node_id) {
        ESP_LOGE(TAG, "Failed to allocate memory for node_id");
        return NULL;
    }
    memcpy(node_id, node_id_start, node_id_len);
    node_id[node_id_len] = '\0';
    return node_id;
}

static void esp_rmaker_controller_params_callback(const char *topic, void *payload, size_t payload_len, void *priv_data)
{
    if (!topic || strlen(topic) == 0 || !payload || payload_len == 0) {
        ESP_LOGE(TAG, "Invalid topic or payload");
        return;
    }

    char *node_id = esp_rmaker_controller_parse_node_id(topic);
    if (!node_id) {
        return;
    }
    /* Only when the callback is not NULL and the node id is not the controller node id, call the callback */
    if (controller_config.cb && strncmp(node_id, esp_rmaker_get_node_id(), strlen(esp_rmaker_get_node_id())) != 0) {
        controller_config.cb(node_id, (char *)payload, payload_len, controller_config.priv_data);
    }
    free(node_id);
}

static void esp_rmaker_controller_create_topic(const char *group_id, char *topic, int topic_size)
{
    memset(topic, 0, topic_size);
    snprintf(topic, topic_size, "node/+/%s/%s", NODE_PARAMS_LOCAL_TOPIC_SUFFIX, group_id);
}

static esp_err_t esp_rmaker_controller_unsubscribe_topic(const char *group_id)
{
    if (!group_id || strlen(group_id) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char topic[MQTT_TOPIC_BUFFER_SIZE] = {0};
    esp_err_t err = ESP_FAIL;
    esp_rmaker_controller_create_topic(group_id, topic, sizeof(topic));
    err = esp_rmaker_mqtt_unsubscribe(topic);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unsubscribe from group topic %s. Error %d", topic, err);
    } else {
        ESP_LOGI(TAG, "Unsubscribed from group topic: %s", topic);
    }
    return err;
}

static esp_err_t esp_rmaker_controller_subscribe_topic(const char *group_id)
{
    if (!group_id || strlen(group_id) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char topic[MQTT_TOPIC_BUFFER_SIZE] = {0};
    esp_err_t err = ESP_FAIL;
    esp_rmaker_controller_create_topic(group_id, topic, sizeof(topic));
    err = esp_rmaker_mqtt_subscribe(topic, esp_rmaker_controller_params_callback, RMAKER_MQTT_QOS1, NULL);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to subscribe to group topic %s. Error %d", topic, err);
    } else {
        ESP_LOGI(TAG, "Subscribed to group topic: %s", topic);
    }
    return err;
}

static esp_err_t bulk_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_write_req_t write_req[], uint8_t count, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    ESP_LOGI(TAG, "Controller received %d params in write", count);
    for (int i = 0; i < count; i++) {
        const esp_rmaker_param_t *param = write_req[i].param;
        esp_rmaker_param_val_t val = write_req[i].val;
        const char *param_name = esp_rmaker_param_get_name(param);
        if (strcmp(param_name, ESP_RMAKER_DEF_USER_TOKEN_NAME) == 0) {
            ESP_LOGI(TAG, "Received value = %s for %s", (char *)val.val.s, param_name);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_BASE_URL_NAME) == 0) {
            ESP_LOGI(TAG, "Received value = %s for %s", (char *)val.val.s, param_name);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_GROUP_ID_NAME) == 0) {
            ESP_LOGI(TAG, "Received value = %s for %s", (char *)val.val.s, param_name);
            /* Unsubscribe from the old group topic if the source is not INIT */
            if (ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
                esp_rmaker_controller_unsubscribe_topic(esp_rmaker_controller_get_active_group_id());
            }
            esp_rmaker_controller_subscribe_topic((char *)val.val.s);
        } else {
            ESP_LOGI(TAG, "Ignoring update for %s", param_name);
        }
        if (ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
            esp_rmaker_param_update(param, val);
        }
    }
    return ESP_OK;
}

esp_err_t esp_rmaker_controller_enable(esp_rmaker_controller_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (controller_service) {
        return ESP_ERR_INVALID_STATE;
    }

    controller_service = esp_rmaker_create_user_auth_service(ESP_RMAKER_DEF_CONTROLLER_SERVICE, bulk_write_cb, NULL, NULL);
    if (!controller_service) {
        ESP_LOGE(TAG, "Failed to create controller service");
        return ESP_FAIL;
    }

    esp_rmaker_device_add_param(controller_service, esp_rmaker_group_id_param_create(ESP_RMAKER_DEF_GROUP_ID_NAME, ""));
    esp_err_t err = esp_rmaker_node_add_device(esp_rmaker_get_node(), controller_service);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add controller service to node");
        esp_rmaker_device_delete(controller_service);
        controller_service = NULL;
        return err;
    }
    controller_config.cb = config->cb;
    controller_config.priv_data = config->priv_data;
    if (config->report_node_details) {
        /* Report the node details */
        err = esp_rmaker_report_node_details();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to report node details");
        }
    }
    return ESP_OK;
}

esp_err_t esp_rmaker_controller_disable(void)
{
    if (!controller_service) {
        return ESP_ERR_INVALID_STATE;
    }

    /* First unsubscribe from the active group topic */
    esp_err_t err = esp_rmaker_controller_unsubscribe_topic(esp_rmaker_controller_get_active_group_id());
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to unsubscribe from active group topic");
    }

    /* Remove the controller service from the node */
    err = esp_rmaker_node_remove_device(esp_rmaker_get_node(), controller_service);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to remove controller service from node");
        return err;
    }

    /* Delete the controller service */
    err = esp_rmaker_device_delete(controller_service);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to delete controller service");
        return err;
    }

    controller_service = NULL;

    /* Report the node details */
    err = esp_rmaker_report_node_details();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to report node details");
    }
    return err;
}

static char *esp_rmaker_controller_get_value_by_name(const char *name)
{
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_name(controller_service, name);
    if (!param) {
        ESP_LOGE(TAG, "Failed to get value by name: %s", name);
        return NULL;
    }
    esp_rmaker_param_val_t *val = esp_rmaker_param_get_val(param);
    if (!val) {
        ESP_LOGE(TAG, "Failed to get value by name: %s", name);
        return NULL;
    }
    if (val->type != RMAKER_VAL_TYPE_STRING) {
        ESP_LOGE(TAG, "Value by name: %s is not a string", name);
        return NULL;
    }
    return val->val.s;
}

char *esp_rmaker_controller_get_user_token(void)
{
    return esp_rmaker_controller_get_value_by_name(ESP_RMAKER_DEF_USER_TOKEN_NAME);
}

char *esp_rmaker_controller_get_base_url(void)
{
    return esp_rmaker_controller_get_value_by_name(ESP_RMAKER_DEF_BASE_URL_NAME);
}

char *esp_rmaker_controller_get_active_group_id(void)
{
    return esp_rmaker_controller_get_value_by_name(ESP_RMAKER_DEF_GROUP_ID_NAME);
}

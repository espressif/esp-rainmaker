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
#pragma once
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <json_generator.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_cmd_resp.h>
#include <esp_idf_version.h>

#define RMAKER_PARAM_FLAG_VALUE_CHANGE   (1 << 0)
#define RMAKER_PARAM_FLAG_VALUE_NOTIFY   (1 << 1)
#define ESP_RMAKER_NVS_PART_NAME            "nvs"

/* Internal margin for parameter buffer allocation */
#define RMAKER_PARAMS_SIZE_MARGIN       50 /* To accommodate for changes in param values while creating JSON */

/* Minimum valid JSON params object size - length of '{"D":{"P":1}}' */
#define RMAKER_MIN_VALID_PARAMS_SIZE    13

typedef enum {
    ESP_RMAKER_STATE_DEINIT = 0,
    ESP_RMAKER_STATE_INIT_DONE,
    ESP_RMAKER_STATE_STARTING,
    ESP_RMAKER_STATE_STARTED,
    ESP_RMAKER_STATE_STOP_REQUESTED,
} esp_rmaker_state_t;

typedef struct {
    esp_rmaker_param_val_t min;
    esp_rmaker_param_val_t max;
    esp_rmaker_param_val_t step;
} esp_rmaker_param_bounds_t;

typedef struct {
    uint8_t str_list_cnt;
    const char **str_list;
} esp_rmaker_param_valid_str_list_t;

struct esp_rmaker_param {
    char *name;
    char *type;
    uint8_t flags;
    uint8_t prop_flags;
    char *ui_type;
    esp_rmaker_param_val_t val;
    esp_rmaker_param_bounds_t *bounds;
    esp_rmaker_param_valid_str_list_t *valid_str_list;
    struct esp_rmaker_device *parent;
    struct esp_rmaker_param * next;
    uint16_t ttl_days;  /* TTL in days for simple time series data */
};
typedef struct esp_rmaker_param _esp_rmaker_param_t;

struct esp_rmaker_attr {
    char *name;
    char *value;
    struct esp_rmaker_attr *next;
};
typedef struct esp_rmaker_attr esp_rmaker_attr_t;


struct esp_rmaker_device {
    char *name;
    char *type;
    char *subtype;
    char *model;
    uint8_t param_count;
    esp_rmaker_device_write_cb_t write_cb;
    esp_rmaker_device_read_cb_t read_cb;
    esp_rmaker_device_bulk_write_cb_t bulk_write_cb;
    esp_rmaker_device_bulk_read_cb_t bulk_read_cb;
    void *priv_data;
    bool is_service;
    esp_rmaker_attr_t *attributes;
    _esp_rmaker_param_t *params;
    _esp_rmaker_param_t *primary;
    const esp_rmaker_node_t *parent;
    struct esp_rmaker_device *next;
};
typedef struct esp_rmaker_device _esp_rmaker_device_t;

typedef struct {
    char *node_id;
    esp_rmaker_node_info_t *info;
    esp_rmaker_attr_t *attributes;
    _esp_rmaker_device_t *devices;
} _esp_rmaker_node_t;

esp_rmaker_node_t *esp_rmaker_node_create(const char *name, const char *type);
esp_err_t esp_rmaker_change_node_id(char *node_id, size_t len);
esp_err_t esp_rmaker_report_value(const esp_rmaker_param_val_t *val, char *key, json_gen_str_t *jptr);
esp_err_t esp_rmaker_report_data_type(esp_rmaker_val_type_t type, char *data_type_key, json_gen_str_t *jptr);
esp_err_t esp_rmaker_report_node_config(void);
esp_err_t esp_rmaker_report_node_state(void);
_esp_rmaker_device_t *esp_rmaker_node_get_first_device(const esp_rmaker_node_t *node);
esp_rmaker_attr_t *esp_rmaker_node_get_first_attribute(const esp_rmaker_node_t *node);
esp_err_t esp_rmaker_params_mqtt_init(void);
esp_err_t esp_rmaker_param_get_stored_value(_esp_rmaker_param_t *param, esp_rmaker_param_val_t *val);
esp_err_t esp_rmaker_param_store_value(_esp_rmaker_param_t *param);
esp_err_t esp_rmaker_node_delete(const esp_rmaker_node_t *node);
esp_err_t esp_rmaker_param_delete(const esp_rmaker_param_t *param);
esp_err_t esp_rmaker_attribute_delete(esp_rmaker_attr_t *attr);
char *esp_rmaker_get_node_config(void);
char *esp_rmaker_get_node_params(void);
esp_err_t esp_rmaker_handle_set_params(char *data, size_t data_len, esp_rmaker_req_src_t src);
esp_err_t esp_rmaker_populate_params(char *buf, size_t *buf_len, uint8_t flags, bool reset_flags);
esp_err_t esp_rmaker_param_cmd_resp_enable(void);
esp_err_t esp_rmaker_user_mapping_prov_init(void);
esp_err_t esp_rmaker_user_mapping_prov_deinit(void);
esp_err_t esp_rmaker_user_node_mapping_init(void);
esp_err_t esp_rmaker_user_node_mapping_deinit(void);
#ifdef CONFIG_ESP_RMAKER_FACTORY_RESET_REPORTING
esp_err_t esp_rmaker_reset_user_node_mapping(void);
#endif
esp_err_t esp_rmaker_init_local_ctrl_service(void);
esp_err_t esp_rmaker_start_local_ctrl_service(const char *serv_name);
esp_err_t esp_rmaker_set_network_id(const char *network_id);
char *esp_rmaker_get_network_id(void);
static inline esp_err_t esp_rmaker_post_event(esp_rmaker_event_t event_id, void* data, size_t data_size)
{
    return esp_event_post(RMAKER_EVENT, event_id, data, data_size, portMAX_DELAY);
}
esp_rmaker_state_t esp_rmaker_get_state(void);
esp_err_t esp_rmaker_cmd_response_enable(void);

/** Register for group parameter updates
 *
 * This function handles group subscriptions and manages MQTT topics for group communication.
 * If there's an existing group subscription, it will be unsubscribed before subscribing to the new group.
 *
 * @param[in] pgrp The group identifier string
 *
 * @return ESP_OK on success
 * @return error in case of failure
 */
esp_err_t esp_rmaker_register_for_group_params(const char *pgrp);

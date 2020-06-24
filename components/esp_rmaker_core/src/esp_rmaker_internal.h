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
#define RMAKER_PARAM_FLAG_VALUE_CHANGE   0x01
struct esp_rmaker_param {
    char *type;
    uint8_t flags;
    uint8_t prop_flags;
    bool read_write; /* TODO: replace with permissions */
    char *name;
    char *ui_type;
    esp_rmaker_param_val_t val;
    esp_rmaker_param_val_t min;
    esp_rmaker_param_val_t max;
    esp_rmaker_param_val_t step;
    struct esp_rmaker_device *parent;
    struct esp_rmaker_param * next;
};
typedef struct esp_rmaker_param esp_rmaker_param_t;

struct esp_rmaker_attr {
    char *name;
    char *value;
    struct esp_rmaker_attr *next;
};
typedef struct esp_rmaker_attr esp_rmaker_attr_t;


struct esp_rmaker_device {
    char *name;
    char *type;
    esp_rmaker_param_callback_t cb;
    void *priv_data;
    bool is_service;
    esp_rmaker_attr_t *attributes;
    esp_rmaker_param_t *params;
    esp_rmaker_param_t *primary;
    struct esp_rmaker_device *next;
};
typedef struct esp_rmaker_device esp_rmaker_device_t;

typedef struct {
    esp_rmaker_work_fn_t work_fn;
    void *priv_data;
} esp_rmaker_work_queue_entry_t;

esp_rmaker_device_t *esp_rmaker_get_device(const char *dev_name);
esp_rmaker_param_t *esp_rmaker_get_param(const char *dev_name, const char *name);
esp_rmaker_param_t *esp_rmaker_get_param_by_key(const char *key);
esp_err_t esp_rmaker_report_value(esp_rmaker_param_val_t *val, char *key, json_gen_str_t *jptr);
esp_err_t esp_rmaker_report_data_type(esp_rmaker_val_type_t type, json_gen_str_t *jptr);
esp_err_t esp_rmaker_report_node_config(void);
esp_err_t esp_rmaker_report_node_state(void);
esp_rmaker_device_t *esp_rmaker_get_first_device(void);
esp_rmaker_device_t *esp_rmaker_get_first_service(void);
esp_rmaker_attr_t *esp_rmaker_get_first_node_attribute(void);
esp_rmaker_device_t *esp_rmaker_get_first_device_template(void);
esp_rmaker_param_t *esp_rmaker_get_first_param_template(void);
esp_err_t esp_rmaker_register_for_get_params(void);
esp_err_t esp_rmaker_report_ts_param(esp_rmaker_param_t *param);
esp_err_t esp_rmaker_report_param(void);
esp_err_t esp_rmaker_queue_report_param(esp_rmaker_work_fn_t work_fn, void *priv_data);
esp_err_t esp_rmaker_param_get_stored_value(esp_rmaker_param_t *param, esp_rmaker_param_val_t *val);
esp_err_t esp_rmaker_param_store_value(esp_rmaker_param_t *param);
static inline esp_err_t esp_rmaker_post_event(esp_rmaker_event_t event_id, void* data, size_t data_size)
{
    return esp_event_post(RMAKER_EVENT, event_id, data, data_size, portMAX_DELAY);
}

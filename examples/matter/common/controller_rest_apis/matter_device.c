// Copyright 2023 Espressif Systems (Shanghai) PTE LTD
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

#include <esp_log.h>
#include <matter_device.h>

#define TAG "MATTER_DEVICE"

void free_matter_device_list(matter_device_t *dev_list)
{
    matter_device_t *current = dev_list;
    while(current) {
        dev_list = dev_list->next;
        free(current);
        current = dev_list;
    }
}

void print_matter_device_list(matter_device_t *dev_list)
{
    uint16_t dev_index = 0;
    while (dev_list) {
        ESP_LOGI(TAG, "device %d : {", dev_index);
        ESP_LOGI(TAG, "    rainmaker_node_id: %s,", dev_list->rainmaker_node_id);
        ESP_LOGI(TAG, "    matter_node_id: 0x%llX,", dev_list->node_id);
        if (dev_list->is_metadata_fetched) {
            ESP_LOGI(TAG, "    is_rainmaker_device: %s,", dev_list->is_rainmaker_device ? "true" : "false");
            ESP_LOGI(TAG, "    is_online: %s,", dev_list->reachable ? "true" : "false");
            ESP_LOGI(TAG, "    endpoints : [");
            for (size_t i = 0; i < dev_list->endpoint_count; ++i) {
                ESP_LOGI(TAG, "        {");
                ESP_LOGI(TAG, "           endpoint_id: %d,", dev_list->endpoints[i].endpoint_id);
                ESP_LOGI(TAG, "           device_type_id: 0x%lx,", dev_list->endpoints[i].device_type_id);
                ESP_LOGI(TAG, "           device_name: %s,", dev_list->endpoints[i].device_name);
                ESP_LOGI(TAG, "        },");
            }
            ESP_LOGI(TAG, "    ]");
        }
        ESP_LOGI(TAG, "}");
        dev_list = dev_list->next;
        dev_index++;
    }
}

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
#include <stdint.h>
#include <esp_timer.h>
#include <esp_system.h>


static esp_timer_handle_t reboot_timer;

static void esp_rmaker_reboot_cb(void *priv)
{
    esp_restart();
}

esp_err_t esp_rmaker_reboot(uint8_t seconds)
{
    if (reboot_timer) {
        return ESP_FAIL;
    }
    esp_timer_create_args_t reboot_timer_conf = {
        .callback = esp_rmaker_reboot_cb,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "rmaker_reboot_tm"
    };
    if (esp_timer_create(&reboot_timer_conf, &reboot_timer) == ESP_OK) {
        return esp_timer_start_once(reboot_timer, seconds * 1000000U);
    }
    return ESP_FAIL;
}

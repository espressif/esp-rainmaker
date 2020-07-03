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
#include <esp_log.h>
#include "lwip/apps/sntp.h"
#include <esp_rmaker_utils.h>

static const char *TAG = "esp_rmaker_time_sync";

#define REF_TIME    1546300800 /* 01-Jan-2019 00:00:00 */
static bool init_done = false;

esp_err_t esp_rmaker_time_sync_init(esp_rmaker_time_config_t *config)
{
    if (sntp_enabled()) {
        ESP_LOGI(TAG, "SNTP already initialized.");
        init_done = true;
        return ESP_OK;
    }
    char *sntp_server_name;
    if (!config || !config->sntp_server_name) {
        sntp_server_name = CONFIG_ESP_RMAKER_SNTP_SERVER_NAME;
    } else {
        sntp_server_name = config->sntp_server_name;
    }
    ESP_LOGI(TAG, "Initializing SNTP. Using the SNTP server: %s", sntp_server_name);
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, sntp_server_name);
    sntp_init();
    init_done = true;
    return ESP_OK;
}

bool esp_rmaker_time_check(void)
{
    time_t now;
    time(&now);
    if (now > REF_TIME) {
        return true;
    }
    return false;
}

#define DEFAULT_TICKS   (2000 / portTICK_PERIOD_MS) /* 2 seconds in ticks */
esp_err_t esp_rmaker_time_wait_for_sync(uint32_t ticks_to_wait)
{
    if (!init_done) {
        ESP_LOGW(TAG, "Time sync not initialised using 'esp_rmaker_time_sync_init'");
    }
    ESP_LOGW(TAG, "Waiting for time to be synchronized. This may take time.");
    uint32_t ticks_remaining = ticks_to_wait;
    uint32_t ticks = DEFAULT_TICKS;
    while (ticks_remaining > 0) {
        if (esp_rmaker_time_check() == true) {
            break;
        }
        ESP_LOGD(TAG, "Time not synchronized yet. Retrying...");
        ticks = ticks_remaining < DEFAULT_TICKS ? ticks_remaining : DEFAULT_TICKS;
        ticks_remaining -= ticks;
        vTaskDelay(ticks);
    }

    /* Check if ticks_to_wait expired and time is not synchronized yet. */
    if (esp_rmaker_time_check() == false) {
        ESP_LOGE(TAG, "Time not synchronized within the provided ticks: %u", ticks_to_wait);
        return ESP_FAIL;
    }

    /* Get current time */
    struct tm timeinfo;
    char strftime_buf[64];
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current UTC time is: %s", strftime_buf);
    return ESP_OK;
}

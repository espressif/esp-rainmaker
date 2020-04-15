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

static const char *TAG = "esp_rmaker_time_sync";

#define REF_TIME    1546300800 /* 01-Jan-2019 00:00:00 */
#define SNTP_SERVER_NAME     "pool.ntp.org"
#define TIME_RETRY_COUNT    10

void esp_rmaker_time_sync_init(void)
{
    if (sntp_enabled()) {
        return;
    }
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, SNTP_SERVER_NAME);
    sntp_init();
}

esp_err_t esp_rmaker_time_sync()
{
    int retry_count = TIME_RETRY_COUNT;
    time_t now;
    while (retry_count--) {
        time(&now);
        if (now > REF_TIME) {
            break;
        }
        ESP_LOGW(TAG, "Time not synchronised yet. Retrying...");
        vTaskDelay(2000 / portTICK_RATE_MS);
        continue;
    }
    if (now < REF_TIME) {
        ESP_LOGE(TAG, "Failed to get current time");
        return ESP_FAIL;
    }
    struct tm timeinfo;
    char strftime_buf[64];
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in London is: %s", strftime_buf);
    return ESP_OK;
}

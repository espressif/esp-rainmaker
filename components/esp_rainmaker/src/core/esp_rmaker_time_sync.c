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
#include <string.h>
#include <esp_log.h>
#include <nvs.h>
#include <lwip/apps/sntp.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_services.h>
#include <esp_rmaker_utils.h>

static const char *TAG = "esp_rmaker_time_sync";

#define ESP_RMAKER_NVS_PART_NAME            "nvs"

#define ESP_RMAKER_TIME_SERV_NAME           "Time"
#define ESP_RMAKER_NVS_TIME_NAMESPACE       "rmaker_time"
#define ESP_RMAKER_TZ_POSIX_NVS_NAME        "tz_posix"
#define ESP_RMAKER_TZ_NVS_NAME              "tz"

#define REF_TIME    1546300800 /* 01-Jan-2019 00:00:00 */
static bool init_done = false;
extern const char *esp_rmaker_tz_db_get_posix_str(const char *name);

#define ESP_RMAKER_DEF_TZ   CONFIG_ESP_RMAKER_DEF_TIMEZONE

esp_err_t esp_rmaker_get_local_time_str(char *buf, size_t buf_len)
{
    struct tm timeinfo;
    char strftime_buf[64];
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c %z[%Z]", &timeinfo);
    size_t print_size = snprintf(buf, buf_len, "%s, DST: %s", strftime_buf, timeinfo.tm_isdst ? "Yes" : "No");
    if (print_size >= buf_len) {
        ESP_LOGE(TAG, "Buffer size %d insufficient for localtime string. REquired size: %d", buf_len, print_size);
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

static esp_err_t esp_rmaker_print_current_time(void)
{
    char local_time[64];
    if (esp_rmaker_get_local_time_str(local_time, sizeof(local_time)) == ESP_OK) {
        if (esp_rmaker_time_check() == false) {
            ESP_LOGI(TAG, "Time not synchronised yet.");
        }
        ESP_LOGI(TAG, "The current time is: %s.", local_time);
        return ESP_OK;
    }
    return ESP_FAIL;
}

static char *__esp_rmaker_time_get_nvs(const char *key)
{
    char *val = NULL;
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, ESP_RMAKER_NVS_TIME_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return NULL;
    }
    size_t len = 0;
    if ((err = nvs_get_blob(handle, key, NULL, &len)) == ESP_OK) {
        val = calloc(1, len + 1); /* +1 for NULL termination */
        if (val) {
            nvs_get_blob(handle, key, val, &len);
        }
    }
    nvs_close(handle);
    return val;

}

static esp_err_t __esp_rmaker_time_set_nvs(const char *key, const char *val)
{
    nvs_handle handle;
    esp_err_t err = nvs_open_from_partition(ESP_RMAKER_NVS_PART_NAME, ESP_RMAKER_NVS_TIME_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(handle, key, val, strlen(val));
    nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static char *esp_rmaker_time_get_timezone_posix(void)
{
    return __esp_rmaker_time_get_nvs(ESP_RMAKER_TZ_POSIX_NVS_NAME);
}

static char *esp_rmaker_time_get_timezone(void)
{
    return __esp_rmaker_time_get_nvs(ESP_RMAKER_TZ_NVS_NAME);
}

esp_err_t esp_rmaker_time_set_timezone_posix(const char *tz_posix)
{
    esp_err_t err = __esp_rmaker_time_set_nvs(ESP_RMAKER_TZ_POSIX_NVS_NAME, tz_posix);
    if (err == ESP_OK) {
        setenv("TZ", tz_posix, 1);
        tzset();
        esp_rmaker_print_current_time();
    }
    return err;
}

esp_err_t esp_rmaker_time_set_timezone(const char *tz)
{
    const char *tz_posix = esp_rmaker_tz_db_get_posix_str(tz);
    if (!tz_posix) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = esp_rmaker_time_set_timezone_posix(tz_posix);
    if (err == ESP_OK) {
        err = __esp_rmaker_time_set_nvs(ESP_RMAKER_TZ_NVS_NAME, tz);
    }
    return err;
}

esp_err_t esp_rmaker_timezone_enable(void)
{
    char *tz_posix = esp_rmaker_time_get_timezone_posix();
    if (tz_posix) {
        setenv("TZ", tz_posix, 1);
        tzset();
        free(tz_posix);
    } else {
        if (strlen(ESP_RMAKER_DEF_TZ) > 0) {
            const char *tz_def = esp_rmaker_tz_db_get_posix_str(ESP_RMAKER_DEF_TZ);
            if (tz_def) {
                setenv("TZ", tz_def, 1);
                tzset();
                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "Invalid Timezone %s specified.", ESP_RMAKER_DEF_TZ);
                return ESP_ERR_INVALID_ARG;
            }
        }
    }
    return ESP_OK;
}
static void esp_rmaker_time_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "SNTP Synchronised.");
    esp_rmaker_print_current_time();
}

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
    if (config && config->sync_time_cb) {
        sntp_set_time_sync_notification_cb(config->sync_time_cb);
    } else {
        sntp_set_time_sync_notification_cb(esp_rmaker_time_sync_cb);
    }
    esp_rmaker_timezone_enable();
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
    esp_rmaker_print_current_time();
    return ESP_OK;
}

static esp_err_t esp_rmaker_time_service_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
        const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    esp_err_t err = ESP_FAIL;
    if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_TIMEZONE) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.s, esp_rmaker_device_get_name(device), esp_rmaker_param_get_name(param));
        err = esp_rmaker_time_set_timezone(val.val.s);
        if (err == ESP_OK) {
            char *tz_posix = esp_rmaker_time_get_timezone_posix();
            if (tz_posix) {
                esp_rmaker_param_t *tz_posix_param = esp_rmaker_device_get_param_by_type(
                        device, ESP_RMAKER_PARAM_TIMEZONE_POSIX);
                esp_rmaker_param_update_and_report(tz_posix_param, esp_rmaker_str(tz_posix));
                free(tz_posix);
            }
        }
    } else if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_TIMEZONE_POSIX) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.s, esp_rmaker_device_get_name(device), esp_rmaker_param_get_name(param));
        err = esp_rmaker_time_set_timezone_posix(val.val.s);
    }
    if (err == ESP_OK) {
        esp_rmaker_param_update_and_report(param, val);
    }
    return err;
}

static esp_err_t esp_rmaker_time_add_service(const char *tz, const char *tz_posix)
{
    esp_rmaker_device_t *service = esp_rmaker_time_service_create(ESP_RMAKER_TIME_SERV_NAME, tz, tz_posix, NULL);
    if (!service) {
        ESP_LOGE(TAG, "Failed to create Time Service");
        return ESP_FAIL;
    }
    esp_rmaker_device_add_cb(service, esp_rmaker_time_service_cb, NULL);
    esp_err_t err = esp_rmaker_node_add_device(esp_rmaker_get_node(), service);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Time service enabled");
    }
    return err;
}

esp_err_t esp_rmaker_timezone_service_enable(void)
{
    char *tz_posix = esp_rmaker_time_get_timezone_posix();
    char *tz = esp_rmaker_time_get_timezone();
    esp_err_t err = esp_rmaker_time_add_service(tz, tz_posix);
    if (tz_posix) {
        free(tz_posix);
    }
    if (tz) {
        free(tz);
    }
    return err;
}

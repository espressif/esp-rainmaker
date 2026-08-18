/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <app_controller.h>
#include <app_insights.h>
#include <app_network.h>
#include <app_rmaker_matter_controller.h>
#include <cJSON.h>
#include <esp_check.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_rmaker_auth_service.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_services.h>
#include <nvs_flash.h>

#if CONFIG_OPENTHREAD_BORDER_ROUTER
#include <esp_ot_config.h>
#include <esp_rmaker_thread_br.h>
#endif

static const char *TAG = "app_main";

static void *cjson_malloc(size_t size)
{
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}

static void app_network_event_handler([[maybe_unused]] void *arg, esp_event_base_t event_base, int32_t event_id,
                                      void *event_data)
{
    if (event_base != APP_NETWORK_EVENT) {
        return;
    }

    switch (event_id) {
    case APP_NETWORK_EVENT_QR_DISPLAY:
        ESP_LOGI(TAG, "Provisioning QR payload: %s", (const char *)event_data);
        break;
    case APP_NETWORK_EVENT_PROV_TIMEOUT:
        ESP_LOGW(TAG, "Network provisioning timed out");
        break;
    case APP_NETWORK_EVENT_PROV_RESTART:
        ESP_LOGW(TAG, "Network provisioning restarted");
        break;
    case APP_NETWORK_EVENT_PROV_CRED_MISMATCH:
        ESP_LOGW(TAG, "Network provisioning credentials mismatch");
        break;
    default:
        break;
    }
}

static void on_device_list_update(esp_err_t err, [[maybe_unused]] const matter_device_t *dev_list)
{
    ESP_RETURN_VOID_ON_FALSE(err == ESP_OK, TAG, "Device list update failed: %s", esp_err_to_name(err));
    ESP_LOGI(TAG, "Matter device list updated");
}

extern "C" void app_main()
{
    /* RainMaker Matter controller keeps large cJSON trees for device metadata and attribute reports.
     * Controller apps with PSRAM should install cJSON hooks before creating any cJSON object so these
     * long-lived allocations prefer PSRAM instead of consuming scarce internal heap. */
    cJSON_Hooks hooks = {
        .malloc_fn = cjson_malloc,
        .free_fn = free
    };
    cJSON_InitHooks(&hooks);

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize network provisioning. */
    app_network_init();
    ESP_ERROR_CHECK(esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID, &app_network_event_handler, NULL));

    /* Initialize the ESP RainMaker Agent. */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Controller");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    /* Enable system service. */
    esp_rmaker_system_serv_config_t system_serv_config = {
        .flags = SYSTEM_SERV_FLAGS_ALL,
        .reboot_seconds = 0,
        .reset_seconds = 2,
        .reset_reboot_seconds = 0,
    };
    esp_rmaker_system_service_enable(&system_serv_config);

    /* Create Matter controller device and parameters. */
    esp_rmaker_device_t *device = esp_rmaker_device_create("MatterController", "matter-controller", NULL);
    ESP_ERROR_CHECK(app_controller_set_device_params(device));
    ESP_ERROR_CHECK(esp_rmaker_node_add_device(node, device));

    /* Enable ESP RainMaker services. */
    esp_rmaker_ota_enable_default();
    esp_rmaker_timezone_service_enable();
    esp_rmaker_schedule_enable();
    esp_rmaker_scenes_enable();
    app_insights_enable();

#if CONFIG_OPENTHREAD_BORDER_ROUTER
    /* Enable Thread Border Router service. */
    esp_openthread_platform_config_t thread_cfg = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    ESP_ERROR_CHECK(esp_rmaker_thread_br_enable(&thread_cfg));
#endif

    /* Initialize Matter controller. */
    ESP_ERROR_CHECK(app_controller_init(on_device_list_update));

    /* Start ESP RainMaker. */
    esp_rmaker_auth_service_enable();
    esp_rmaker_start();

    /* Start network provisioning. */
    ESP_ERROR_CHECK(app_network_set_custom_mfg_data(MFG_DATA_DEVICE_TYPE_MATTER_CONTROLLER,
                                                    MFG_DATA_DEVICE_SUBTYPE_MATTER_CONTROLLER));
    err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wi-Fi. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    /* Start Matter. */
    ESP_ERROR_CHECK(esp_matter::start(NULL));
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::init();
}

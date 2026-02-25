/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* ESP RainMaker Controller example: controller node with CLI for User API */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_controller.h>
#include <esp_rmaker_auth_service.h>
#include <app_network.h>
#include <app_insights.h>

#include "app_priv.h"

static const char *TAG = "app_main";

static esp_err_t controller_cb(const char *node_id, const char *data, size_t data_size, void *priv_data)
{
    ESP_LOGI(TAG, "Controller received data from node %s: %.*s", node_id, data_size, data);
    return ESP_OK;
}

void app_main()
{
    app_driver_init();

    /* Initialize NVS */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize CLI commands */
    app_cli_command_init();

    /* Initialize Wi-Fi/Thread (must be called before esp_rmaker_node_init()) */
    app_network_init();

    /* Initialize ESP RainMaker Agent (after app_network_init(), before app_network_start()) */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Controller", "Controller");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Add README URL to node info */
    if (strlen(CONFIG_EXAMPLE_README_URL) > 0) {
        esp_rmaker_node_add_readme(node, CONFIG_EXAMPLE_README_URL);
    }

    /* Add controller device to the node (for app device icon) */
    esp_rmaker_device_t *controller_device = esp_rmaker_device_create("RMController", "esp.device.controller", NULL);
    esp_rmaker_device_add_param(controller_device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "ESP-RM-Controller"));
    esp_rmaker_node_add_device(node, controller_device);

    /* Enable RainMaker controller service */
    esp_rmaker_controller_config_t controller_config = {
        .report_node_details = false,
        .cb = controller_cb,
        .priv_data = NULL,
    };
    esp_rmaker_controller_enable(&controller_config);

    /* Enable RainMaker auth service */
    esp_rmaker_auth_service_enable();

    /* Enable OTA (default) */
    esp_rmaker_ota_enable_default();

    /* Enable timezone service (required for scheduling from phone app).
     * See https://rainmaker.espressif.com/docs/time-service.html
     */
    esp_rmaker_timezone_service_enable();

    /* Enable scheduling */
    esp_rmaker_schedule_enable();

    /* Enable Scenes */
    esp_rmaker_scenes_enable();

    /* Enable system service (reboot, reset, etc.) */
    esp_rmaker_system_serv_config_t system_serv_config = {
        .flags = SYSTEM_SERV_FLAGS_ALL,
        .reboot_seconds = 2,
        .reset_seconds = 2,
        .reset_reboot_seconds = 2,
    };
    esp_rmaker_system_service_enable(&system_serv_config);

    /* Enable Insights (requires CONFIG_ESP_INSIGHTS_ENABLED=y) */
    app_insights_enable();

    /* Start ESP RainMaker Agent */
    esp_rmaker_start();

    /* Start Wi-Fi/Thread (returns after connection or provisioning) */
    err = app_network_start((app_network_pop_type_t) CONFIG_APP_POP_TYPE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start network. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}

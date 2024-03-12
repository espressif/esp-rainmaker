/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app_insights.h>
#include <app_matter.h>
#include <app_priv.h>
#include <app_reset.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_types.h>
#include <nvs_flash.h>

#include "box_main.h"

#include "matter_ctrl_service.h"

static const char *TAG = "app_main";

bool rmaker_init_done = false;

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }

    const char *param_name = esp_rmaker_param_get_name(param);
    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        app_matter_report_power(val.val.b);
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

extern "C" void app_main()
{
    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize drivers for button */
    app_driver_handle_t button_handle = app_driver_button_init(NULL);
    app_reset_button_register(button_handle);

    /* Initialize matter */
    app_matter_init();
    app_matter_endpoint_create();

    /* Matter start */
    app_matter_start();

    /* Initialize the ESP RainMaker Agent.
     */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Controller");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node.");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    esp_rmaker_device_t *device = esp_rmaker_device_create(CONTROLLER_DEVICE_NAME, ESP_RMAKER_DEVICE_SOCKET, NULL);
    if (device) {
        esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, CONTROLLER_DEVICE_NAME));
        esp_rmaker_param_t *primary = esp_rmaker_power_param_create(ESP_RMAKER_DEF_POWER_NAME, DEFAULT_POWER);
        esp_rmaker_device_add_param(device, primary);
        esp_rmaker_device_assign_primary_param(device, primary);
    }
    esp_rmaker_device_add_cb(device, write_cb, NULL);

    esp_rmaker_node_add_device(node, device);

    /* Enable timezone service which will be require for setting appropriate timezone
     * from the phone apps for scheduling to work correctly.
     * For more information on the various ways of setting timezone, please check
     * https://rainmaker.espressif.com/docs/time-service.html.
     */
    esp_rmaker_timezone_service_enable();

    /* Enable scheduling. */
    esp_rmaker_schedule_enable();

    /* Enable Scenes */
    esp_rmaker_scenes_enable();

    esp_rmaker_controller_service_enable();

    /* Enable Insights. Requires CONFIG_ESP_INSIGHTS_ENABLED=y */
    app_insights_enable();

    /* Pre start */
    ESP_ERROR_CHECK(app_matter_pre_rainmaker_start());

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();
    rmaker_init_done = true;

    /* Enable Matter diagnostics console*/
    app_matter_enable_matter_console();

    /* Enable Timer for device update */
    update_device_refresh_ui_init();

    /* Start box */
    box_main();
}

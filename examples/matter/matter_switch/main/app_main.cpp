/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <app_priv.h>
#include <app_matter.h>
#include <app_reset.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_ota.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <app_insights.h>

static const char *TAG = "app_main";

static app_driver_handle_t switch_handle;

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }

    const char *param_name = esp_rmaker_param_get_name(param);
    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        app_matter_send_command_binding(val.val.b);
    }
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

    /* Initialize drivers for light and button */
    switch_handle = app_driver_light_init();
    app_driver_switch_set_power(switch_handle, DEFAULT_POWER);
    app_driver_handle_t button_handle = app_driver_button_init(switch_handle);
    app_reset_button_register(button_handle);

    /* Initialize matter */
    app_matter_init();
    app_matter_switch_create(switch_handle);

    /* Matter start */
    app_matter_start();

    /* Initialize the ESP RainMaker Agent.
     * Create Lightbulb device and its parameters.
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Switch");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node.");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
    esp_rmaker_device_t *switch_device = esp_rmaker_switch_device_create(SWITCH_DEVICE_NAME, NULL, DEFAULT_POWER);
    esp_rmaker_device_add_cb(switch_device, write_cb, NULL);


    esp_rmaker_node_add_device(node, switch_device);

    /* Enable OTA */
    esp_rmaker_ota_config_t ota_config = {
        .server_cert = ESP_RMAKER_OTA_DEFAULT_SERVER_CERT,
    };
    esp_rmaker_ota_enable(&ota_config, OTA_USING_PARAMS);

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

    /* Enable Insights. Requires CONFIG_ESP_INSIGHTS_ENABLED=y */
    app_insights_enable();

    /* Pre start */
    ESP_ERROR_CHECK(app_matter_pre_rainmaker_start());

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    /* Enable Matter diagnostics console*/
    app_matter_enable_matter_console();
}

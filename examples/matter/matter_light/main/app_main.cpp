/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <app_insights.h>
#include <app_reset.h>
#include <esp_rmaker_console.h>

#include "app_priv.h"
#include "app_matter.h"

static const char *TAG = "app_main";


static app_driver_handle_t light_handle;

bool rmaker_init_done = false; // used with extern in `app_matter.c`

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }

    const char *param_name = esp_rmaker_param_get_name(param);
    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        app_driver_light_set_power(light_handle, val.val.b);
        app_matter_report_power(val.val.b);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_HUE_NAME) == 0) {
        app_driver_light_set_hue(light_handle, val.val.i);
        app_matter_report_hue(val.val.i);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_SATURATION_NAME) == 0) {
        app_driver_light_set_saturation(light_handle, val.val.i);
        app_matter_report_saturation(val.val.i);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_CCT_NAME) == 0) {
        app_driver_light_set_temperature(light_handle, val.val.i);
        app_matter_report_temperature(val.val.i);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
        app_driver_light_set_brightness(light_handle, val.val.i);
        app_matter_report_brightness(val.val.i);
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
    ESP_ERROR_CHECK( err );

    /* Initialize drivers for light and button */
    light_handle = app_driver_light_init();
    app_driver_handle_t button_handle = app_driver_button_init(light_handle);
    app_reset_button_register(button_handle);
    esp_rmaker_console_init();

    /* Initialize matter */
    app_matter_init();
    app_matter_light_create(light_handle);

    /* Matter start */
    app_matter_start();

    /* Starting driver with default values */
    app_driver_light_set_defaults();

    /* Initialize the ESP RainMaker Agent.
     * Create Lightbulb device and its parameters.
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Lightbulb");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node.");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    esp_rmaker_device_t *light_device = esp_rmaker_lightbulb_device_create(LIGHT_DEVICE_NAME, NULL, DEFAULT_POWER);
    esp_rmaker_device_add_cb(light_device, write_cb, NULL);

    esp_rmaker_device_add_param(light_device, esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, DEFAULT_BRIGHTNESS));
    esp_rmaker_device_add_param(light_device, esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME, DEFAULT_SATURATION));
    esp_rmaker_device_add_param(light_device, esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, DEFAULT_HUE));
    esp_rmaker_device_add_param(light_device, esp_rmaker_cct_param_create(ESP_RMAKER_DEF_CCT_NAME, DEFAULT_TEMPERATURE));

    esp_rmaker_node_add_device(node, light_device);

    /* Enable OTA */
    esp_rmaker_ota_enable_default();

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
    rmaker_init_done = true;
}

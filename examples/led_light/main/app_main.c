/* LED Light Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>

#include <app_wifi.h>
#include <app_insights.h>

#include "app_priv.h"

static const char *TAG = "app_main";

esp_rmaker_device_t *light_device;

#ifdef CONFIG_ESP_RMAKER_CMD_RESP_ENABLE

#include <json_parser.h>
#include <esp_rmaker_cmd_resp.h>
#include <esp_rmaker_standard_types.h>

static char resp_data[100];
/* Callback to handle commands received from the RainMaker cloud via the Command - Response Framework
 *
 * Sample payloads:
 *     - {"on":true}
 *     - {"brightness":30}
 */
esp_err_t led_light_cmd_handler(const void *in_data, size_t in_len, void **out_data, size_t *out_len, esp_rmaker_cmd_ctx_t *ctx, void *priv)
{
    if (in_data == NULL ){
        ESP_LOGE(TAG, "No data received");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Got command: %.*s", in_len, (char *)in_data);
    jparse_ctx_t jctx;
    if (json_parse_start(&jctx, (char *)in_data, in_len) != 0) {
        snprintf(resp_data, sizeof(resp_data), "{\"status\":\"fail\", \"description\":\"invalid json\"}");
    } else {
        int brightness;
        bool on_state;
        if (json_obj_get_int(&jctx, "brightness", &brightness) == 0) {
            if (brightness < 0 || brightness > 100) {
                snprintf(resp_data, sizeof(resp_data), "{\"status\":\"fail\", \"description\":\"out of bounds\"}");
            } else {
                app_light_set_brightness(brightness);
                esp_rmaker_param_update_and_report(
                        esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_BRIGHTNESS),
                        esp_rmaker_int(brightness));
                snprintf(resp_data, sizeof(resp_data), "{\"status\":\"success\"}");
            }
        } else if (json_obj_get_bool(&jctx, "on", &on_state) == 0) {
            app_light_set_power(on_state);
            esp_rmaker_param_update_and_report(
                    esp_rmaker_device_get_param_by_type(light_device, ESP_RMAKER_PARAM_POWER),
                    esp_rmaker_bool(on_state));
            snprintf(resp_data, sizeof(resp_data), "{\"status\":\"success\"}");
        } else {
            snprintf(resp_data, sizeof(resp_data), "{\"status\":\"fail\", \"description\":\"invalid param\"}");
        }
    }
    *out_data = resp_data;
    *out_len = strlen(resp_data);
    return ESP_OK;
}

#endif /* CONFIG_ESP_RMAKER_CMD_RESP_ENABLE */

/* Callback to handle param updates received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    const char *device_name = esp_rmaker_device_get_name(device);
    const char *param_name = esp_rmaker_param_get_name(param);
    if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", device_name, param_name);
        app_light_set_power(val.val.b);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        app_light_set_brightness(val.val.i);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_HUE_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        app_light_set_hue(val.val.i);
    } else if (strcmp(param_name, ESP_RMAKER_DEF_SATURATION_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, device_name, param_name);
        app_light_set_saturation(val.val.i);
    } else {
        /* Silently ignoring invalid params */
        return ESP_OK;
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

void app_main()
{
    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    app_driver_init();

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_wifi_init();
    
    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_wifi_init() but before app_wifi_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Lightbulb");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a device and add the relevant parameters to it */
    light_device = esp_rmaker_lightbulb_device_create("Light", NULL, DEFAULT_POWER);
    esp_rmaker_device_add_cb(light_device, write_cb, NULL);

    esp_rmaker_device_add_param(light_device, esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, DEFAULT_BRIGHTNESS));
    esp_rmaker_device_add_param(light_device, esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, DEFAULT_HUE));
    esp_rmaker_device_add_param(light_device, esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME, DEFAULT_SATURATION));

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

#ifdef CONFIG_ESP_RMAKER_CMD_RESP_ENABLE
    /* Register a command for demonstration */
    esp_rmaker_cmd_register(ESP_RMAKER_CMD_CUSTOM_START, ESP_RMAKER_USER_ROLE_PRIMARY_USER | ESP_RMAKER_USER_ROLE_SECONDARY_USER, led_light_cmd_handler, false, NULL);
#endif

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    err = app_wifi_set_custom_mfg_data(MGF_DATA_DEVICE_TYPE_LIGHT, MFG_DATA_DEVICE_SUBTYPE_LIGHT);
    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_wifi_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }
}

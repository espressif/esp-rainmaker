/* Client-only Matter Controller Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <inttypes.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_event.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_ota.h>

#include <esp_rmaker_common_events.h>

#include <app_wifi.h>
#include <app_insights.h>
#include <app_matter_device_manager.h>

#include <matter_controller_std.h>
#include <app_matter_controller.h>
#include <app_matter_controller_callback.h>
#include <app_matter_controller_creds_issuer.h>
#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_controller_console.h>
#include <esp_matter_controller_client.h>
#include <esp_matter_controller_credentials_issuer.h>

static const char *TAG = "app_main";
esp_rmaker_device_t *matter_controller_device;

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
            const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", esp_rmaker_device_get_name(device),
                esp_rmaker_param_get_name(param));
        esp_rmaker_param_update_and_report(param, val);
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

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_wifi_init();

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_wifi_init() but before app_wifi_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Switch");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a MatterController device.
     * You can optionally use the helper API esp_rmaker_switch_device_create() to
     * avoid writing code for adding the name and power parameters.
     */
    matter_controller_device = esp_rmaker_device_create("MatterController", ESP_RMAKER_DEVICE_MATTER_CONTROLLER, NULL);

    /* Add the write callback for the device. We aren't registering any read callback yet as
     * it is for future use.
     */
    esp_rmaker_device_add_cb(matter_controller_device, write_cb, NULL);

    /* Add the standard name parameter (type: esp.param.name), which allows setting a persistent,
     * user friendly custom name from the phone apps. All devices are recommended to have this
     * parameter.
     */
    esp_rmaker_device_add_param(matter_controller_device,
                                esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, "MatterController"));

    /* Add this switch device to the node */
    esp_rmaker_node_add_device(node, matter_controller_device);

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

    /* Enable Matter Controller service */
    matter_controller_enable(0x131B, app_matter_controller_callback);

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    err = app_wifi_set_custom_mfg_data(MGF_DATA_DEVICE_TYPE_MATTER_CONTROLLER,
                                       MFG_DATA_DEVICE_SUBTYPE_MATTER_CONTROLLER);
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

    // Start matter
    esp_matter::start(NULL);
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    esp_matter::controller::matter_controller_client::get_instance().init(0, 0, 5580);
    esp_matter::lock::chip_stack_unlock();
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::init();
    esp_matter::console::controller_register_commands();
    esp_matter::console::ctl_dev_mgr_register_commands();
    init_device_manager(NULL);
    // Update matter controller handler after join to Wi-Fi network
    matter_controller_handle_update();
}

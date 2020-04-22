/* Multi-Device Example

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

#include <app_wifi.h>

#include "app_priv.h"

static const char *TAG = "app_main";

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t common_callback(const char *dev_name, const char *name, esp_rmaker_param_val_t val, void *priv_data)
{
    if (strcmp(name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                val.val.b? "true" : "false", dev_name, name);
        if (strcmp(dev_name, "Switch") == 0) {
            app_driver_set_state(val.val.b);
        }
    } else if (strcmp(name, "brightness") == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, dev_name, name);
    } else if (strcmp(name, "speed") == 0) {
        ESP_LOGI(TAG, "Received value = %d for %s - %s",
                val.val.i, dev_name, name);
    } else {
        /* Silently ignoring invalid params */
        return ESP_OK;
    }
    esp_rmaker_update_param(dev_name, name, val);
    return ESP_OK;
}

void app_main()
{
    /* Initialize Application specific hardware drivers and
     * set initial state.
     */
    app_driver_init();
    app_driver_set_state(DEFAULT_SWITCH_POWER);

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_init()
     */
    app_wifi_init();
    
    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_wifi_init() but before app_wifi_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .info = {
            .name = "ESP RainMaker Multi Device",
            .type = "Multi Device",
        },
        .enable_time_sync = false,
    };
    err = esp_rmaker_init(&rainmaker_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not initialise ESP RainMaker. Aborting!!!");
        vTaskDelay(5000/portTICK_PERIOD_MS);
        abort();
    }

    /* Create a Switch device and add the relevant parameters to it */
    esp_rmaker_create_switch_device("Switch", common_callback, NULL, DEFAULT_SWITCH_POWER);

    /* Create a Light device and add the relevant parameters to it */
    esp_rmaker_create_lightbulb_device("Light", common_callback, NULL, DEFAULT_LIGHT_POWER);

    esp_rmaker_device_add_brightness_param("Light", "brightness", DEFAULT_LIGHT_BRIGHTNESS);
    esp_rmaker_device_add_attribute("Light", "serial_number", "012345");
    esp_rmaker_device_add_attribute("Light", "mac", "xx:yy:zz:aa:bb:cc");
    
    /* Create a Fan device and add the relevant parameters to it */
    esp_rmaker_create_fan_device("Fan", common_callback, NULL, DEFAULT_FAN_POWER);
    esp_rmaker_device_add_speed_param("Fan", "speed", DEFAULT_FAN_SPEED);

    /* Create a Temperature Sensor device and add the relevant parameters to it */
    esp_rmaker_create_temp_sensor_device("Temperature Sensor", NULL, NULL, app_get_current_temperature());

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    app_wifi_start();
}

/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "sdkconfig.h"
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <app_wifi.h>
#include <app_insights.h>
#include <esp_app_rainmaker.h>
#include <iot_button.h>
#include <esp_rmaker_utils.h>
#include "esp_timer.h"
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_gateway_control.h>

/* Rainmaker app driver: button factory reset */
/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          0 // GPIO0, esp32s3
#define BUTTON_ACTIVE_LEVEL  BUTTON_ACTIVE_LOW
#define WIFI_RESET_BUTTON_TIMEOUT       2
#define FACTORY_RESET_BUTTON_TIMEOUT    3
#define REBOOT_DELAY        2
#define RESET_DELAY         2
static const char *TAG = "esp_app_rainmaker";
esp_rmaker_device_t *zigbee_gw_device;
esp_rainmaker_gateway_end_device_list_t *end_device_list = NULL;

#if !CONFIG_ZIGBEE_INSTALLCODE_ENABLED
static esp_timer_handle_t zigbee_network_open_timer_handle = NULL;
static uint64_t zigbee_network_open_timeout_period_us = ESP_ZIGBEE_GATWAY_OPEN_NETWORK_DEFAULT_TIME * 1000* 1000; // 5 seconds
static void esp_rainmaker_update_zigbee_add_device_state(bool new_state);

static void zigbee_open_network_timer_stop(void *time)
{
    esp_rainmaker_update_zigbee_add_device_state(0);
    ESP_LOGI(TAG, "Zigbe gateway network closed");
    esp_timer_stop(zigbee_network_open_timer_handle);
    esp_timer_delete(zigbee_network_open_timer_handle);
    zigbee_network_open_timer_handle = NULL;
}

static esp_err_t zigbee_networtk_close_start_timer(void)
{
    esp_timer_create_args_t zigbee_open_network_timer_conf = {
        .callback = zigbee_open_network_timer_stop,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "zigbee_networtk_close"
    };
    if (esp_timer_create(&zigbee_open_network_timer_conf, &zigbee_network_open_timer_handle) == ESP_OK) {
        esp_timer_start_once(zigbee_network_open_timer_handle, zigbee_network_open_timeout_period_us);
        ESP_LOGI(TAG, "Zigbee network close after %d seconds", ESP_ZIGBEE_GATWAY_OPEN_NETWORK_DEFAULT_TIME);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to create zigbee network open timer");
    }
    return ESP_FAIL;
}

static void esp_rainmaker_update_zigbee_add_device_state(bool new_state)
{
    esp_rmaker_param_update_and_notify(
        esp_rmaker_device_get_param_by_name(zigbee_gw_device, ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE),
        esp_rmaker_bool(new_state));
}
#endif

esp_rmaker_device_t *esp_rmaker_zigbee_gateway_device_create(const char *dev_name,
        void *priv_data)
{
    esp_rmaker_device_t *device = esp_rmaker_device_create(dev_name, ESP_RMAKER_DEVICE_ZIGBEE_GATEWAY, priv_data);
    if (device) {
        esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, dev_name));
#if CONFIG_ZIGBEE_INSTALLCODE_ENABLED
        esp_rmaker_param_t *param = esp_rmaker_param_create(ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE, ESP_RMAKER_PARAM_ADD_ZIGBEE_DEVICE, esp_rmaker_str(""), PROP_FLAG_READ | PROP_FLAG_WRITE);
        esp_rmaker_param_add_ui_type(param, ESP_RMAKER_UI_QR_SCAN);
#else 
        esp_rmaker_param_t *param = esp_rmaker_param_create(ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE, ESP_RMAKER_PARAM_ADD_ZIGBEE_DEVICE, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
        esp_rmaker_param_add_ui_type(param, ESP_RMAKER_UI_TOGGLE);
#endif // CONFIG_ZIGBEE_INSTALLCODE_ENABLED
        esp_rmaker_device_add_param(device, param);
    }
    return device;
}

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    char *device_name = esp_rmaker_device_get_name(device);
    char *param_name = esp_rmaker_param_get_name(param);
    ESP_LOGI(TAG, "Received device name = %s,parameter name = %s", device_name, param_name);
    if (strcmp(param_name, ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE) == 0) {
#if CONFIG_ZIGBEE_INSTALLCODE_ENABLED
        esp_zigbee_ic_mac_address_t *IC_MacAddress = (esp_zigbee_ic_mac_address_t *)malloc(sizeof(esp_zigbee_ic_mac_address_t));
        if (esp_zb_prase_ic_obj(val.val.s, IC_MacAddress) == ESP_OK) {
            ESP_LOGI(TAG, "prase zigbee ic success");
            ESP_LOGI(TAG, "prase install code:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x CRC:%02x%02x",
                     IC_MacAddress->ic[0], IC_MacAddress->ic[1], IC_MacAddress->ic[2], IC_MacAddress->ic[3],
                     IC_MacAddress->ic[4], IC_MacAddress->ic[5], IC_MacAddress->ic[6], IC_MacAddress->ic[7],
                     IC_MacAddress->ic[8], IC_MacAddress->ic[9], IC_MacAddress->ic[10], IC_MacAddress->ic[11],
                     IC_MacAddress->ic[12], IC_MacAddress->ic[13], IC_MacAddress->ic[14], IC_MacAddress->ic[15],
                     IC_MacAddress->ic[16], IC_MacAddress->ic[17]);
            ESP_LOGI(TAG, "prase MAC address::%02x%02x%02x%02x%02x%02x%02x%02x",
                     IC_MacAddress->addr[7], IC_MacAddress->addr[6], IC_MacAddress->addr[5], IC_MacAddress->addr[4],
                     IC_MacAddress->addr[3], IC_MacAddress->addr[2], IC_MacAddress->addr[1], IC_MacAddress->addr[0]);
            /* Gateway could start permit device to join and add install code */
            esp_gateway_control_permit_join();
            esp_gateway_control_secur_ic_add(IC_MacAddress, ESP_ZB_IC_TYPE_128);
        } else {
            ESP_LOGE(TAG, "prase zigbee ic failed");
        }
 #else
        if (val.val.b == true) {
            ESP_LOGI(TAG, "Zigbee gateway add device");
            esp_gateway_control_permit_join();
            if (zigbee_network_open_timer_handle == NULL) {
                zigbee_networtk_close_start_timer();
            } else {
                ESP_LOGI(TAG, "Failed to start zigbee network open timer");
            }

        }
#endif // CONFIG_ZIGBEE_INSTALLCODE_ENABLED
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static void esp_app_rainmaker_get_zigbee_nwk_info(const esp_rmaker_device_t *device, light_bulb_device_params_t *ret_info)
{
    uint32_t private_data = *(uint32_t *)esp_rmaker_device_get_priv_data(device);
    uint16_t short_address = (uint16_t)(private_data >> 16);
    uint8_t endpoint = (uint8_t)(private_data);
    ret_info->short_addr = short_address;
    ret_info->endpoint = endpoint;
}

static esp_err_t light_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                 val.val.b ? "true" : "false", esp_rmaker_device_get_name(device),
                 esp_rmaker_param_get_name(param));
        /* light on/off control with corresponding short address and endpoint */
        light_bulb_device_params_t device_info;
        esp_app_rainmaker_get_zigbee_nwk_info(device, &device_info);
        esp_gateway_control_light_on_off(val.val.b, &device_info);
        esp_rmaker_param_update_and_report(param, val);
    }
    return ESP_OK;
}

static esp_err_t esp_app_rainmaker_add_light_end_device(uint16_t short_address, uint8_t endpoint)
{
    esp_rainmaker_gateway_end_device_list_t *new = (esp_rainmaker_gateway_end_device_list_t *)malloc(sizeof(esp_rainmaker_gateway_end_device_list_t));
    esp_rainmaker_gateway_end_device_list_t *curr = end_device_list;
    while (curr) {
        if ((curr->zigbee_info.short_addr == short_address) && (curr->zigbee_info.endpoint == endpoint)) {
            free(new);
            ESP_LOGI(TAG, "existing end device back online");
            return ESP_OK;
        }
        if (curr->next) {
            curr = curr->next;
        } else {
            break;
        }
    }
    char light_name[50];
    uint32_t *private_data = (uint32_t *)malloc(sizeof(uint32_t)); /* need to free it if remove device */
    sprintf(light_name, "Light_%x_%d", short_address, endpoint);
    /* form a uin32 type private data as (XX,XX)(00,xx)*/
    *private_data = ((uint32_t)short_address << 16U) + endpoint;
    new->device = esp_rmaker_lightbulb_device_create(light_name, (void *)(private_data), false);
    esp_rmaker_device_add_cb(new->device, light_write_cb, NULL);
    esp_rmaker_node_add_device(esp_rmaker_get_node(), new->device);
    new->device_type = ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID;
    new->zigbee_info.endpoint = endpoint;
    new->zigbee_info.short_addr = short_address;
    new->next = NULL;
    if (curr) {
        curr->next = new;
    } else {
        end_device_list = new;
    }
    return ESP_OK;
}

esp_err_t esp_app_rainmaker_joining_add_device(esp_zb_ha_standard_devices_t device_type, uint16_t short_address, uint8_t endpoint)
{
    switch (device_type) {
    case ESP_ZB_HA_ON_OFF_OUTPUT_DEVICE_ID:
        /* Create a Light device and add the relevant parameters to it */
        esp_app_rainmaker_add_light_end_device(short_address, endpoint);
        if (esp_rmaker_report_node_details() != ESP_OK) {
            ESP_LOGE(TAG, "Report node state failed.");
            return ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "Add a light device success with short_address:0x%x, endpoint:%d", short_address, endpoint);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void wifi_reset_trigger(void *arg)
{
    esp_rmaker_wifi_reset(RESET_DELAY, REBOOT_DELAY);
}

static void wifi_reset_indicate(void *arg)
{
    ESP_LOGI(TAG, "Release button now for Wi-Fi reset. Keep pressed for factory reset.");
}

static void zigbee_reset_trigger(void *arg)
{
    esp_zb_zcl_reset_nvram_to_factory_default(); // zigbee factory reset
}

static void zigbee_reset_indicate(void *arg)
{
    ESP_LOGI(TAG, "Release button now for zigbee reset. Keep pressed for factory reset.");
}

static void factory_reset_trigger(void *arg)
{
    esp_rmaker_factory_reset(RESET_DELAY, REBOOT_DELAY);
}

static void factory_reset_indicate(void *arg)
{
    ESP_LOGI(TAG, "Release button to trigger factory reset.");
}

static esp_err_t app_reset_button_register(button_handle_t btn_handle, uint8_t wifi_reset_timeout,
        uint8_t factory_reset_timeout)
{
    if (!btn_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (wifi_reset_timeout) {
        iot_button_add_on_release_cb(btn_handle, wifi_reset_timeout, wifi_reset_trigger, NULL);
        iot_button_add_on_press_cb(btn_handle, wifi_reset_timeout, wifi_reset_indicate, NULL);

        iot_button_add_on_release_cb(btn_handle, wifi_reset_timeout, zigbee_reset_trigger, NULL);
        iot_button_add_on_press_cb(btn_handle, wifi_reset_timeout, zigbee_reset_indicate, NULL);
    }
    if (factory_reset_timeout) {
        if (factory_reset_timeout <= wifi_reset_timeout) {
            ESP_LOGW(TAG, "It is recommended to have factory_reset_timeout > wifi_reset_timeout");
        }
        iot_button_add_on_release_cb(btn_handle, factory_reset_timeout, factory_reset_trigger, NULL);
        iot_button_add_on_press_cb(btn_handle, factory_reset_timeout, factory_reset_indicate, NULL);
    }
    return ESP_OK;
}

static button_handle_t app_reset_button_create(gpio_num_t gpio_num, button_active_t active_level)
{
    return iot_button_create(gpio_num, active_level);
}

static void app_driver_init()
{
    app_reset_button_register(app_reset_button_create(BUTTON_GPIO, BUTTON_ACTIVE_LEVEL),
                WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
}

void esp_app_rainmaker_main()
{
    app_driver_init(); /* factory button register */

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
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Gateway Device", "GW");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    /* Create a device and add the relevant parameters to it */
    zigbee_gw_device = esp_rmaker_zigbee_gateway_device_create("Zigbee_Gateway", NULL);
    esp_rmaker_device_add_cb(zigbee_gw_device, write_cb, NULL);
    esp_rmaker_node_add_device(node, zigbee_gw_device);

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

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_wifi_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
}

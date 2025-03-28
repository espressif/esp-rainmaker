/*!
 * @brief An integrated Multi-Device using StickCplus Example

 * @copyright This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
 */

#include "Arduino.h"

extern "C" 
{
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>

#include <app_insights.h>
#include <esp_log.h>
#include <freertos/task.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_scenes.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_standard_devices.h>
#include <json_parser.h>
}

#include "app_priv.h"
#include "interface_module/m5_interface.h"

static const char *TAG = "app_main";

esp_rmaker_node_t *stickCplus;
esp_err_t err;

esp_rmaker_device_t *switch_device;
esp_rmaker_device_t *light_device;
esp_rmaker_device_t *relay_device;

/*!
 *  @brief Callback to handle param updates received from the RainMaker cloud.
 *  @param device Pointer to ESP RainMaker device.
    @param write_req Array of parameter write requests.
    @param count To count the number of params that needs to be updated.
    @param priv_data Pointer to private data associated with the specific device.
    @param ctx Identifies write request source and provides context for handling operations.
    */
static esp_err_t bulk_write_cb(const esp_rmaker_device_t *device,
                               const esp_rmaker_param_write_req_t write_req[], uint8_t count, void *priv_data,
                               esp_rmaker_write_ctx_t *ctx) 
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    ESP_LOGI(TAG, "Light received %d params in write", count);
    for (int i = 0; i < count; i++) {
        const esp_rmaker_param_t *param = write_req[i].param;
        esp_rmaker_param_val_t val = write_req[i].val;
        const char *device_name = esp_rmaker_device_get_name(device);
        const char *param_name = esp_rmaker_param_get_name(param);

        ESP_LOGI(TAG, "Received value = %d for %s - %s", val.val.i, device_name, param_name);

        if (device == switch_device) {
            if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
                rm_sgl_sw_state = val.val.b;
                if (is_sgl_sw_scrn) {
                    set_sgl_sw_state(val.val.b);
                }
            }
        } else if (device == light_device) {
            if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
                rm_light_sw_state = val.val.b;
                if (is_light_scrn) {
                    set_light_sw_state(val.val.b);
                }
            } else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
                brt_level = val.val.i;
                if (is_light_scrn) {
                    set_light_brt(val.val.i);
                }
            } else if (strcmp(param_name, ESP_RMAKER_DEF_HUE_NAME) == 0) {
                hue_level = val.val.i;
                if (is_light_scrn) {
                    set_light_hue(val.val.i);
                }
            }
        } else if (device == relay_device) {
            if (strcmp(param_name, "switch_1") == 0) {
                rm_relay_sw_state[RELAY_SW_1] = val.val.i;
            } else if (strcmp(param_name, "switch_2") == 0) {
                rm_relay_sw_state[RELAY_SW_2] = val.val.i;
            } else if (strcmp(param_name, "switch_3") == 0) {
                rm_relay_sw_state[RELAY_SW_3] = val.val.i;
            } else if (strcmp(param_name, "switch_4") == 0) {
                rm_relay_sw_state[RELAY_SW_4] = val.val.i;
            }
            if (is_relay_scrn) {
                set_relay_sw_state();
                set_relay();
            }
        } else {
            /* Silently ignoring invalid params */
            return ESP_OK;
        }
        ESP_LOGI(TAG, "Updating for %s", param_name);
        esp_rmaker_param_update(param, val);
    }
    return ESP_OK;
}

/*!
 *  @brief Task to handle M5 button events with periodic checks.
 *  @param pvParameter Pointer to any parameters passed to the task.
 */
static void button_task(void *pvParameter) 
{
    while (true) {
        check_button_events();
        vTaskDelay(20 / portTICK_PERIOD_MS); // Small delay to yield to other tasks
    }
}

/*!
 *  @brief Creates a switch parameter with the given name and default power state.
 *  @param param_name The name of the switch parameter to create.
 *  @return A pointer to the created switch parameter.
 */
static esp_rmaker_param_t *create_switch_param(const char *param_name) 
{
    return esp_rmaker_param_create(param_name, ESP_RMAKER_PARAM_TOGGLE, esp_rmaker_bool(DEFAULT_SWITCH_POWER),
                                   PROP_FLAG_READ | PROP_FLAG_WRITE);
}

/*!
 *  @brief Initializes the NVS (Non-Volatile Storage) for the device.
 */
static void init_nvs(void) 
{
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

/*!
 *  @brief Initialize the ESP RainMaker Agent.
 *  @note  Note that this function should be called after app_network_init() but before
 *         app_network_start() in start_wifi().
 */
static void init_rmaker_agent(void) 
{
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    stickCplus = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Multi Device", "Multi Device");
    if (!stickCplus) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
}

/*! @brief Start the Wi-Fi.
 *  @note  If the node is provisioned, it will start connection attempts,
 *         else, it will start Wi-Fi provisioning. The function will return
 *         after a connection has been successfully established. This is when the qrcode is display.
 */
static void start_wifi(void) 
{
    err = app_network_start(POP_TYPE_RANDOM);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
    del_prov_display(); 
}

/*!
 *  @brief Sets up a switch device and adds it to the node.
 *  @param node A pointer to the ESP RainMaker node to which the device will be added.
 */
static void setup_sw_device(esp_rmaker_node_t *node) 
{
    switch_device = esp_rmaker_switch_device_create("Single Switch", NULL, DEFAULT_SWITCH_POWER);
    esp_rmaker_device_add_bulk_cb(switch_device, bulk_write_cb, NULL);
    esp_rmaker_node_add_device(node, switch_device);
}

/*!
 *  @brief Sets up a light device and adds parameters (e.g., brightness and hue) to the node.
 *  @param node A pointer to the ESP RainMaker node to which the device will be added.
 */
static void setup_light_device(esp_rmaker_node_t *node) 
{
    light_device = esp_rmaker_lightbulb_device_create("Light Switch", NULL, DEFAULT_SWITCH_POWER);
    esp_rmaker_device_add_param(light_device, esp_rmaker_brightness_param_create(
                                                  ESP_RMAKER_DEF_BRIGHTNESS_NAME, DEFAULT_RM_BRIGHTNESS));
    esp_rmaker_device_add_param(light_device,
                                esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, DEFAULT_RM_HUE));
    /* Add device attributes (optional) */
    // esp_rmaker_device_add_attribute(light_device, "Serial Number", "012345");
    // esp_rmaker_device_add_attribute(light_device, "MAC",
    // "xx:yy:zz:aa:bb:cc");
    esp_rmaker_device_add_bulk_cb(light_device, bulk_write_cb, NULL);
    esp_rmaker_node_add_device(node, light_device);
}

/*!
 *  @brief Sets up a relay device with multiple switch parameters.
 *  @param node A pointer to the ESP RainMaker node to which the relay device will be added.
 */
static void setup_relay_device(esp_rmaker_node_t *node) 
{
    relay_device = esp_rmaker_device_create("Relay Switch", NULL, NULL);

    esp_rmaker_param_t *name_param = esp_rmaker_param_create(
        "name", NULL, esp_rmaker_str("Relay_device"), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    esp_rmaker_param_t *power_param_sw1 = create_switch_param("switch_1");
    esp_rmaker_param_t *power_param_sw2 = create_switch_param("switch_2");
    esp_rmaker_param_t *power_param_sw3 = create_switch_param("switch_3");
    esp_rmaker_param_t *power_param_sw4 = create_switch_param("switch_4");

    esp_rmaker_param_add_ui_type(power_param_sw1, ESP_RMAKER_UI_TOGGLE);
    esp_rmaker_param_add_ui_type(power_param_sw2, ESP_RMAKER_UI_TOGGLE);
    esp_rmaker_param_add_ui_type(power_param_sw3, ESP_RMAKER_UI_TOGGLE);
    esp_rmaker_param_add_ui_type(power_param_sw4, ESP_RMAKER_UI_TOGGLE);

    esp_rmaker_device_add_param(relay_device, name_param);
    esp_rmaker_device_add_param(relay_device, power_param_sw1);
    esp_rmaker_device_add_param(relay_device, power_param_sw2);
    esp_rmaker_device_add_param(relay_device, power_param_sw3);
    esp_rmaker_device_add_param(relay_device, power_param_sw4);

    esp_rmaker_device_add_bulk_cb(relay_device, bulk_write_cb, NULL);
    esp_rmaker_node_add_device(node, relay_device);
}

/*!
 *  @brief Handles application network events and processes provisioning QR codes.
 *         This function listens for application network events and processes provisioning-related events. 
 *         When a QR code is received (APP_NETWORK_EVENT_QR_DISPLAY), it logs and displays the QR code, 
 *         extracts the Proof of Possession (PoP) from the JSON payload, and displays it if available. 
 *         It also handles provisioning timeouts and restarts due to failures.
 *  @param arg A pointer to user-defined data passed to the event handler.
 *  @param event_base The base of the event that was triggered.
 *  @param event_id The specific event identifier within the event base.
 *  @param event_data A pointer to the event-specific data.
 *
 */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == APP_NETWORK_EVENT) {
        switch (event_id) {
            case APP_NETWORK_EVENT_QR_DISPLAY:
            {
                ESP_LOGI(TAG, "Provisioning QR : %s", (char *)event_data);
                display_qrcode_m5((const char*)event_data);
                jparse_ctx_t jctx;
                int ret;
                json_parse_start(&jctx, (char *)event_data, strlen((char *)event_data));
                char pop_str[20];
                ret = json_obj_get_string(&jctx, "pop", pop_str, sizeof(pop_str));
                if (ret != -1) {
                    display_pop_m5(pop_str);
                }
                json_parse_end(&jctx);
            }
            break;
            case APP_NETWORK_EVENT_PROV_TIMEOUT:
                ESP_LOGI(TAG, "Provisioning Timed Out. Please reboot.");
                break;
            case APP_NETWORK_EVENT_PROV_RESTART:
                ESP_LOGI(TAG, "Provisioning has restarted due to failures.");
                break;
            default:
                ESP_LOGW(TAG, "Unhandled App Wi-Fi Event: %"PRIi32, event_id);
                break;
        }
    }
}

extern "C" void app_main() 
{
    init_app_driver();
    set_lighting_state(DEFAULT_SWITCH_POWER);
    xTaskCreate(button_task, "button_status_task", 8192, NULL, 1, NULL);
    
    /* This is a generic flow of how to run RainMaker. */
    init_nvs();
    app_network_init();  // Init Wi-Fi

    // Init event handler
    esp_event_handler_register(APP_NETWORK_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);

    init_rmaker_agent(); // Called after app_network_init() but before app_network_start() in
                         // start_wifi().
    setup_sw_device(stickCplus);
    setup_light_device(stickCplus);
    setup_relay_device(stickCplus);
    esp_rmaker_ota_enable_default(); // Enable OTA
    esp_rmaker_timezone_service_enable(); // For setting appropriate timezone from the phone apps.
                                          // In order for scheduling to work correctly.
                                          // For more infomation on ways to set timezone visit,
                                          // https://rainmaker.espressif.com/docs/time-service.html.
    esp_rmaker_schedule_enable(); // Enable scheduling
    esp_rmaker_scenes_enable(); // Enable Scenes
    app_insights_enable(); // Enable ESP Insights, but not used in this example.
                           // Requires CONFIG_ESP_INSIGHTS_ENABLED=y
    esp_rmaker_start(); // Start ESP RainMaker Agent.
    start_wifi();
}

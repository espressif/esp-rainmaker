/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_common_events.h>
#include <app_network.h>
#include <iot_button.h>
#include <button_gpio.h>
#include <app_reset.h>
#include "esp_rmaker_console.h"
#include "esp_rmaker_common_console.h"

#include "rmaker_camera.h"
#include "esp_cli.h"
#include "network_coprocessor.h"
#include "bridge_peer_connection.h"
#include "esp_work_queue.h"
#include "sleep_command.h"

static const char *TAG = "app_main";

/* Configuration */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0
#define WIFI_RESET_BUTTON_TIMEOUT       5
#define FACTORY_RESET_BUTTON_TIMEOUT    10

/* Device name prefix */
static const char *device_name = "WebRTC Camera";

/* Global reference to camera device for parameter updates */
static esp_rmaker_device_t *g_camera_device = NULL;

/* Update channel parameter with new node ID */
static void update_channel_param(void)
{
    if (!g_camera_device) {
        ESP_LOGW(TAG, "Camera device not initialized, cannot update channel parameter");
        return;
    }

    const char *node_id = esp_rmaker_get_node_id();
    if (!node_id) {
        ESP_LOGE(TAG, "Failed to get node ID for channel parameter update");
        return;
    }

    /* Get the channel parameter */
    esp_rmaker_param_t *channel_param = esp_rmaker_device_get_param_by_name(
        g_camera_device, ESP_RMAKER_DEF_CHANNEL_NAME);
    if (!channel_param) {
        ESP_LOGE(TAG, "Channel parameter not found");
        return;
    }

    /* Create new channel name with updated node ID */
    char channel_name[32] = {0};
    snprintf(channel_name, sizeof(channel_name), "%s%s", ESP_RMAKER_CHANNEL_NAME_PREFIX, node_id);

    /* Update and report the parameter */
    esp_err_t err = esp_rmaker_param_update_and_report(channel_param, esp_rmaker_str(channel_name));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Channel parameter updated to: %s", channel_name);
    } else {
        ESP_LOGE(TAG, "Failed to update channel parameter: %d", err);
    }
}

/* Event handler for RainMaker common and RainMaker events */
static void rainmaker_event_handler(void *arg, esp_event_base_t event_base,
                                    int32_t event_id, void *event_data)
{
    if (event_base == RMAKER_COMMON_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_REBOOT:
                ESP_LOGI(TAG, "Rebooting in %d seconds.", *((uint8_t *)event_data));
                break;
            case RMAKER_EVENT_WIFI_RESET:
                ESP_LOGI(TAG, "Wi-Fi credentials reset.");
                break;
            case RMAKER_EVENT_FACTORY_RESET:
                ESP_LOGI(TAG, "Node reset to factory defaults.");
                break;
            case RMAKER_MQTT_EVENT_CONNECTED:
                ESP_LOGI(TAG, "MQTT Connected.");
                break;
            case RMAKER_MQTT_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "MQTT Disconnected.");
                break;
            case RMAKER_MQTT_EVENT_PUBLISHED:
                ESP_LOGI(TAG, "MQTT Published. Msg id: %d.", *((int *)event_data));
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Common Event: %"PRIi32, event_id);
                break;
        }
    } else if (event_base == RMAKER_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_CLAIM_SUCCESSFUL:
                ESP_LOGI(TAG, "RainMaker Claim Successful. Updating channel parameter...");
                /* Node ID may have changed during claiming, update channel parameter */
                update_channel_param();
                break;
            default:
                ESP_LOGW(TAG, "Unhandled RainMaker Event: %"PRIi32, event_id);
                break;
        }
    } else {
        ESP_LOGW(TAG, "Invalid event received!");
    }
}

#if CONFIG_IDF_TARGET_ESP32P4
/* Work-around for BLE transport when using hosted on ESP32P4 */
void ble_transport_ll_init(void)
{
    /* No-op for hosted workaround */
}

void ble_transport_ll_deinit(void)
{
    /* No-op for hosted workaround */
}
#endif

/* Initialize the RainMaker node and camera device */
static esp_err_t initialize_rainmaker_device(void)
{
    esp_rmaker_config_t config = {
        .enable_time_sync = false,
    };

    /* Initialize the RainMaker node */
    esp_rmaker_node_t *node = esp_rmaker_node_init(&config, device_name, "Camera");
    if (!node) {
        ESP_LOGE(TAG, "Failed to initialize RainMaker node");
        return ESP_FAIL;
    }

    /* Create the device name with chip target */
    char chip_target[32] = {0};
    strcpy(chip_target, CONFIG_IDF_TARGET);
    for (int i = 0; chip_target[i]; i++) {
        chip_target[i] = toupper((unsigned char)chip_target[i]);
    }
    strcat(chip_target, " Camera");

    /* Create the camera device (automatically includes name and channel parameters) */
    g_camera_device = esp_rmaker_camera_device_create(chip_target, NULL);
    if (!g_camera_device) {
        ESP_LOGE(TAG, "Failed to create camera device");
        return ESP_FAIL;
    }

    /* Add the device to the node */
    if (esp_rmaker_node_add_device(node, g_camera_device) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device to node");
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* Mode-specific initialization callback for split mode */
static int split_mode_init_callback(void *user_data)
{
    (void)user_data;

    /* Initialize work queue in advance with lower (than default) stack size */
    esp_work_queue_config_t work_queue_config = ESP_WORK_QUEUE_CONFIG_DEFAULT();
    work_queue_config.stack_size = 12 * 1024;
    if (esp_work_queue_init_with_config(&work_queue_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        return -1;
    }

    if (esp_work_queue_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        return -1;
    }

    /* Register sleep commands for power management */
    if (sleep_command_register_cli() != 0) {
        ESP_LOGE(TAG, "Failed to register sleep commands");
        return -1;
    }

    return 0;
}

/* Initialize and register the reset button */
static void initialize_reset_button(void)
{
    button_config_t btn_cfg = {
        .long_press_time = 0,  /* Use default */
        .short_press_time = 0, /* Use default */
    };
    button_gpio_config_t gpio_cfg = {
        .gpio_num = BUTTON_GPIO,
        .active_level = BUTTON_ACTIVE_LEVEL,
        .enable_power_save = false,
    };
    button_handle_t btn_handle = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle) == ESP_OK && btn_handle) {
        /* Register reset button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    }
}

void app_main(void)
{
    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP32 WebRTC Camera Example");

    /* Initialize ESP CLI */
    esp_cli_start();

    /* Initialize Wi-Fi - must be called before network_coprocessor_init */
    app_network_init();

    /* Initialize the network coprocessor */
    network_coprocessor_init();

    /* Register an event handler to catch RainMaker common events */
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_COMMON_EVENT, ESP_EVENT_ANY_ID,
                                               &rainmaker_event_handler, NULL));

    /* Register an event handler for RainMaker events (including claim events) */
    ESP_ERROR_CHECK(esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID,
                                               &rainmaker_event_handler, NULL));

    /* Initialize RainMaker device */
    initialize_rainmaker_device();

    /* Start RainMaker */
    if (esp_rmaker_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start RainMaker");
        return;
    }

    /* Initialize the RainMaker console */
    esp_rmaker_console_init();
    esp_rmaker_common_register_commands();

    /* Initialize reset button */
    initialize_reset_button();

    /* Start the Wi-Fi and wait for connection */
    ESP_LOGI(TAG, "Starting Wi-Fi provisioning or connection...");
    ret = app_network_start(POP_TYPE_RANDOM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wi-Fi. Aborting!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }
    ESP_LOGI(TAG, "Network connection established");

    /* Initialize and start WebRTC */
    ESP_LOGI(TAG, "Starting WebRTC...");

    /* Configure WebRTC camera with split mode interfaces */
    rmaker_webrtc_camera_config_t camera_config = {
        .peer_connection_if = bridge_peer_connection_if_get(),
        .video_capture = NULL,
        .audio_capture = NULL,
        .init_callback = split_mode_init_callback,
        .init_callback_user_data = NULL,
    };

    rmaker_webrtc_camera_init(&camera_config);

    ESP_LOGI(TAG, "Main application continuing after starting WebRTC");
}

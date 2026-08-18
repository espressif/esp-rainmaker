/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ctype.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_common_events.h>
#include <app_network.h>
#include <iot_button.h>
#include <button_gpio.h>
#include <app_reset.h>
#include "esp_rmaker_console.h"

#include "rmaker_camera.h"
#include "esp_cli.h"
#include "kvs_peer_connection.h"
#include "media_stream.h"

#if CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE
/* On P4 the BLE controller lives on the C6 co-processor and is driven over the
 * esp_hosted VHCI transport; it must be explicitly brought up before BLE
 * provisioning (NimBLE cannot init a local controller on the P4). */
#include "esp_hosted.h"
#endif

#ifdef CONFIG_SLAVE_FLASHER_ENABLE
#include "slave_flasher.h"
static vprintf_like_t s_original_vprintf = NULL;

/* Prefix co-processor flashing logs with [HOST] so they're easy to tell apart
 * from the slave's own output that slave_flasher streams back over UART. */
static int custom_vprintf(const char *fmt, va_list args)
{
    printf("\033[1;36m[HOST]\033[0m ");
    return s_original_vprintf ? s_original_vprintf(fmt, args) : vprintf(fmt, args);
}
#endif

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

/* Boot-time co-processor flash: retry a few times to ride out a transient
 * UART glitch before giving up (each attempt is a no-op unless the MD5 differs). */
#define SLAVE_FLASH_MAX_ATTEMPTS 3

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

#ifdef CONFIG_SLAVE_FLASHER_ENABLE
    /* On single-PCB P4 boards (e.g. P4-EYE) the on-board C6/C5 co-processor is
     * flashed in-system: write the embedded network_adapter image over UART
     * before bringing up the hosted SDIO link. Must run before app_network_init(). */
    s_original_vprintf = esp_log_set_vprintf(custom_vprintf);
    /* flash_slave() re-flashes only on MD5 mismatch, so a warm reboot is a no-op. */
    for (int attempt = 1; attempt <= SLAVE_FLASH_MAX_ATTEMPTS; attempt++) {
        ret = flash_slave();
        if (ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "Co-processor flash attempt %d/%d failed: %s",
                 attempt, SLAVE_FLASH_MAX_ATTEMPTS, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    esp_log_set_vprintf(s_original_vprintf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to flash co-processor after %d attempts: %s",
                 SLAVE_FLASH_MAX_ATTEMPTS, esp_err_to_name(ret));
        return;
    }
#endif

    /* Initialize ESP CLI */
    esp_cli_start();

    /* Initialize Wi-Fi */
    app_network_init();

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

    /* Initialize reset button */
    initialize_reset_button();

#if CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE
    /* On P4 the BLE controller lives on the C6 co-processor and is reached over
     * the esp_hosted VHCI transport. Bring it up here (at the example layer,
     * before BLE provisioning starts) since NimBLE cannot init a local
     * controller on the P4. It is left up after provisioning: the only safe
     * teardown signal (NimBLE fully stopped) isn't exposed by app_network, and
     * deinit'ing earlier races the NimBLE host shutdown. */
    if (esp_hosted_bt_controller_init() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to init co-processor BT controller");
    }
    if (esp_hosted_bt_controller_enable() != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable co-processor BT controller");
    }
#endif

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

    /* Configure WebRTC camera with standalone mode interfaces */
    rmaker_webrtc_camera_config_t camera_config = {
        .peer_connection_if = kvs_peer_connection_if_get(),
        .video_capture = media_stream_get_video_capture_if(),
        .audio_capture = media_stream_get_audio_capture_if(),
        .init_callback = NULL,
        .init_callback_user_data = NULL,
    };

    rmaker_webrtc_camera_init(&camera_config);

    ESP_LOGI(TAG, "Main application continuing after starting WebRTC");
}

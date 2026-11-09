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
#include <esp_rmaker_time_sync.h>
#include <esp_sntp.h>
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
#include "eh_cp.h"
#include "esp_netif.h"
#include "power_save_handler.h"
#include "host_ps_integration.h"
#include "bridge_peer_connection.h"
#include "esp_work_queue.h"
#include "sleep_command.h"
#if CONFIG_IDF_TARGET_ESP32C5 && CONFIG_BT_LE_LP_CLK_SRC_MAIN_XTAL
#include <network_provisioning/manager.h>
#include "esp_private/esp_modem_clock.h"
#endif

static const char *TAG = "app_main";

/* Configuration */
#define BUTTON_GPIO          CONFIG_EXAMPLE_BOARD_BUTTON_GPIO
#define BUTTON_ACTIVE_LEVEL  0
#if CONFIG_ESP_HOSTED_CP_FEAT_HOST_PS && CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_ENABLE
#define BUTTON_IRQ_WAKE      true
#else
#define BUTTON_IRQ_WAKE      false
#endif
/* How long signaling waits for the first SNTP sync after the network is up */
#define TIME_SYNC_WAIT_MS    15000
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
    /* KVS signs every request with the system clock, so it needs real time */
    esp_rmaker_config_t config = {
        .enable_time_sync = true,
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

    /* Reboot and factory reset from the phone app. The C5 has no reset button,
     * and its console loses reset-to-factory after a host wake. */
    esp_rmaker_system_serv_config_t system_serv_config = {
        .flags = SYSTEM_SERV_FLAGS_ALL,
        .reboot_seconds = 2,
        .reset_seconds = 2,
        .reset_reboot_seconds = 2,
    };
    if (esp_rmaker_system_service_enable(&system_serv_config) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable the system service");
    }

    return ESP_OK;
}

/* esp_hosted's CLI restart on a host wake clears the console command table and adds back
 * only its own commands, so register the app's again. */
static void register_cli_after_wake(void)
{
    esp_cli_register_cmds();
    esp_rmaker_common_register_commands();
    esp_rmaker_register_commands();
    (void)sleep_command_register_cli();
}

/* Mode-specific initialization callback for split mode */
static int split_mode_init_callback(void *user_data)
{
    (void)user_data;

    /* 12 KB overflowed by ~640 B in the SDP-answer / signaling handler on
     * esp_workq_task (stack-protection fault at streaming start). */
    esp_work_queue_config_t work_queue_config = ESP_WORK_QUEUE_CONFIG_DEFAULT();
    work_queue_config.stack_size = 16 * 1024;
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

static void set_backup_dns(esp_netif_t *netif)
{
    esp_netif_dns_info_t dns = { 0 };

    if (CONFIG_EXAMPLE_BACKUP_DNS_SERVER[0] == '\0') {
        return;
    }
    /* One unanswered query fails a signaling call. DHCP's server first. */
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns) != ESP_OK ||
        dns.ip.u_addr.ip4.addr == 0) {
        dns.ip.type = ESP_IPADDR_TYPE_V4;
        if (esp_netif_str_to_ip4(CONFIG_EXAMPLE_BACKUP_DNS_SERVER, &dns.ip.u_addr.ip4) != ESP_OK) {
            ESP_LOGW(TAG, "Invalid backup DNS server: %s", CONFIG_EXAMPLE_BACKUP_DNS_SERVER);
            return;
        }
        esp_netif_set_dns_info(netif, ESP_NETIF_DNS_BACKUP, &dns);
    }
}

static void on_got_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    set_backup_dns(((ip_event_got_ip_t *)data)->esp_netif);

    /* SNTP starts with RainMaker, before there is a network. Its first request
     * fails, and lwIP sends the next one only after 15 s. Send it now. */
    if (esp_sntp_enabled() && !esp_rmaker_time_check()) {
        esp_sntp_restart();
    }
}

#if CONFIG_IDF_TARGET_ESP32C5 && CONFIG_BT_LE_LP_CLK_SRC_MAIN_XTAL
/* BLE init keeps the main XTAL on through light sleep for its low-power clock.
 * On the C5, esp_bt_controller_deinit() does not release it: IDF has the
 * modem_clock_deselect_lp_clock_source(PERIPH_BT_MODULE) calls commented out in
 * components/bt/controller/esp32c5/bt.c (lines 1115 and 1140 in v5.5.5 and
 * v6.0.3). The C6 controller makes the same calls. Without this, every light
 * sleep after BLE provisioning costs about 4-5 mA more. app_network's handler,
 * registered first, has torn BLE down by now.
 * IDF fixes this in "fix(bt): fix deselect slow clk missing on ESP32-C5"
 * It is in v5.5.6 and v6.1.1, and on release/v6.0 after v6.0.3. With that
 * fix this second call does nothing more. Remove it once the minimum supported
 * IDF has the fix. */
static void on_prov_end(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    modem_clock_deselect_lp_clock_source(PERIPH_BT_MODULE);
    ESP_LOGI(TAG, "Provisioning ended: released the BLE XTAL clock");
}
#endif

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
        /* With CP light sleep, wake on a GPIO interrupt: a polled button runs a
         * 20 ms timer, which keeps the co-processor out of light sleep. The
         * interrupt handler needs CONFIG_GPIO_CTRL_FUNC_IN_IRAM, or a press
         * during a flash write panics. Without light sleep, poll. */
        .enable_power_save = BUTTON_IRQ_WAKE,
    };
    button_handle_t btn_handle = NULL;
    if (iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_handle) == ESP_OK && btn_handle) {
        /* Register reset button */
        app_reset_button_register(btn_handle, WIFI_RESET_BUTTON_TIMEOUT, FACTORY_RESET_BUTTON_TIMEOUT);
    } else {
        /* A GPIO that cannot wake the chip from light sleep is rejected. On the
         * ESP32-C5 only GPIO 0-6 can, so the boot button on GPIO 28 is not used. */
        ESP_LOGW(TAG, "No reset button: GPIO %d cannot wake the chip from light sleep", BUTTON_GPIO);
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

    /* Creates the default STA netif, which esp_hosted's network split picks up */
    app_network_init();

    /* Bring up the esp_hosted co-processor. AUTO_INIT is off so it comes after
     * the netif above. */
    ESP_ERROR_CHECK(eh_cp_init());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_got_ip, NULL));
#if CONFIG_IDF_TARGET_ESP32C5 && CONFIG_BT_LE_LP_CLK_SRC_MAIN_XTAL
    ESP_ERROR_CHECK(esp_event_handler_register(NETWORK_PROV_EVENT, NETWORK_PROV_END, &on_prov_end, NULL));
#endif

    /* Before anything can ask the co-processor to sleep */
    host_ps_integration_set_cli_register_cb(register_cli_after_wake);
    power_save_init();

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

    /* With the clock still at 1970 KVS rejects the signed requests, and
     * signaling only gets through after a clock-skew retry. */
    if (esp_rmaker_time_wait_for_sync(pdMS_TO_TICKS(TIME_SYNC_WAIT_MS)) != ESP_OK) {
        ESP_LOGW(TAG, "Time not synchronised after %d ms; starting signaling anyway",
                 TIME_SYNC_WAIT_MS);
    }

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

    /* The bridge adapter wakes the P4 for an offer. The media-start wake does
     * not fire in split mode yet (see power_save_handler.c). */
    power_save_register_webrtc_events();

    rmaker_webrtc_camera_init(&camera_config);

    ESP_LOGI(TAG, "Main application continuing after starting WebRTC");
}

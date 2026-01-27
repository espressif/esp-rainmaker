/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * On-Network Challenge-Response User-Node Mapping Example
 *
 * This file demonstrates how to start the on-network challenge-response
 * service when the device gets an IP address on the network.
 *
 * Supports two methods:
 * 1. CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE - Standalone HTTP service
 * 2. CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE - Via Local Control service
 *
 * The service automatically stops after a successful mapping when the
 * CLI sends the disable command (timer-based cleanup).
 *
 * Usage:
 * 1. Enable one of the config options above in menuconfig
 * 2. Call app_on_network_chal_resp_init() from your app_main()
 * 3. Build and flash the device
 * 4. Use esp-rainmaker-cli to discover and map the device:
 *    esp-rainmaker-cli provision --transport on-network
 */

#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_mac.h>
#include <esp_rmaker_core.h>

#ifdef CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE
#include <esp_rmaker_on_network_chal_resp.h>
#endif
/* Note: esp_rmaker_local_ctrl_service_started() and esp_rmaker_local_ctrl_enable_chal_resp()
 * are declared in esp_rmaker_core.h, which is already included above */

static const char *TAG = "on_network_chal_resp";

#if defined(CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE) || defined(CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE)

static bool s_service_started = false;
static bool s_got_ip = false;

/*
 * Helper function to enable challenge-response for local control
 */
#ifdef CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE
static void try_enable_local_ctrl_chal_resp(void)
{
    if (s_service_started) {
        return;
    }

    if (!s_got_ip) {
        ESP_LOGD(TAG, "Waiting for IP address before enabling challenge-response");
        return;
    }

    if (!esp_rmaker_local_ctrl_service_started()) {
        ESP_LOGD(TAG, "Local control service not started yet");
        return;
    }

    /* Build instance name as PROV_xxyyzz using last 3 bytes of Ethernet MAC */
    char instance_name[16];
    uint8_t mac[6];
    if (esp_read_mac(mac, ESP_MAC_ETH) == ESP_OK) {
        snprintf(instance_name, sizeof(instance_name), "PROV_%02X%02X%02X", mac[3], mac[4], mac[5]);
    } else {
        snprintf(instance_name, sizeof(instance_name), "PROV_000000");
    }

    ESP_LOGI(TAG, "Enabling challenge-response for local control (instance: %s)...", instance_name);
    esp_err_t err = esp_rmaker_local_ctrl_enable_chal_resp(instance_name);
    if (err == ESP_OK) {
        s_service_started = true;
        ESP_LOGI(TAG, "Challenge-response enabled for local control");
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "**************************************************");
        ESP_LOGI(TAG, "  On-network provisioning available");
        ESP_LOGI(TAG, "  Service name: %s", instance_name);
        ESP_LOGI(TAG, "  PoP: %s", CONFIG_EXAMPLE_LOCAL_CTRL_POP);
        ESP_LOGI(TAG, "**************************************************");
        ESP_LOGI(TAG, "");
    } else {
        ESP_LOGE(TAG, "Failed to enable challenge-response: %s", esp_err_to_name(err));
    }
}
#endif

/*
 * Event handler for IP events (both Wi-Fi and Ethernet) to start the service
 */
static void on_got_ip_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    const char *network_type = "Unknown";
    
    if (event_id == IP_EVENT_STA_GOT_IP) {
        network_type = "Wi-Fi";
    } else if (event_id == IP_EVENT_ETH_GOT_IP) {
        network_type = "Ethernet";
    }
    
    ESP_LOGI(TAG, "Got IP on %s: " IPSTR, network_type, IP2STR(&event->ip_info.ip));

    s_got_ip = true;

    if (s_service_started) {
        ESP_LOGD(TAG, "Service already started");
        return;
    }

#ifdef CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE
    ESP_LOGI(TAG, "Starting On-Network Challenge-Response service...");

    /* Use default config - customize as needed */
    esp_rmaker_on_network_chal_resp_config_t config = ESP_RMAKER_ON_NETWORK_CHAL_RESP_DEFAULT_CONFIG();

    /* Optional: Override defaults */
    /* config.port = 8080; */
    /* config.sec_ver = ESP_RMAKER_ON_NETWORK_SEC1; */
    /* config.pop = "my_secret_pop"; */

    esp_err_t err = esp_rmaker_on_network_chal_resp_start(&config);
    if (err == ESP_OK) {
        s_service_started = true;
        ESP_LOGI(TAG, "Service started - device can be discovered via mDNS");
    } else {
        ESP_LOGE(TAG, "Failed to start service: %s", esp_err_to_name(err));
    }
#elif defined(CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE)
    try_enable_local_ctrl_chal_resp();
#endif
}

/*
 * Event handler for RMAKER_EVENT_LOCAL_CTRL_STARTED
 */
#ifdef CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE
static void on_local_ctrl_started_handler(void *arg, esp_event_base_t event_base,
                                         int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Local control service started");
    try_enable_local_ctrl_chal_resp();
}
#endif

/*
 * Initialize on-network challenge-response
 * Call this from app_main() after network initialization (app_ethernet_init() and/or app_network_init())
 */
esp_err_t app_on_network_chal_resp_init(void)
{
    esp_err_t err;
    
    /* Register for Ethernet IP events */
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP,
                                     &on_got_ip_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Ethernet IP event handler: %s", esp_err_to_name(err));
        return err;
    }

#ifdef CONFIG_EXAMPLE_ENABLE_WIFI
    /* Register for Wi-Fi IP events if Wi-Fi is enabled */
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                     &on_got_ip_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register Wi-Fi IP event handler: %s", esp_err_to_name(err));
        return err;
    }
#endif

#ifdef CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE
    ESP_LOGI(TAG, "Initialized - on-network challenge-response service will start when device gets IP (Wi-Fi or Ethernet)");
#elif defined(CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE)
    err = esp_event_handler_register(RMAKER_EVENT, RMAKER_EVENT_LOCAL_CTRL_STARTED,
                                     &on_local_ctrl_started_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register local control event handler: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "Initialized - challenge-response will be enabled for local control when device gets IP and local control starts");
#endif
    return ESP_OK;
}

#else /* Neither config option enabled */

esp_err_t app_on_network_chal_resp_init(void)
{
    ESP_LOGW(TAG, "On-Network Challenge-Response not enabled");
    ESP_LOGW(TAG, "Enable CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE or");
    ESP_LOGW(TAG, "CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE in menuconfig");
    return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CONFIG_ESP_RMAKER_ON_NETWORK_CHAL_RESP_ENABLE || CONFIG_ESP_RMAKER_LOCAL_CTRL_CHAL_RESP_ENABLE */


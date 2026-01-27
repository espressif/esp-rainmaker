/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_check.h"
#include "sdkconfig.h"

#include "app_ethernet.h"

static const char *TAG = "app_ethernet";

static esp_eth_handle_t s_eth_handle = NULL;
static esp_netif_t *s_eth_netif = NULL;
static esp_eth_netif_glue_handle_t s_eth_netif_glue = NULL;
static EventGroupHandle_t s_eth_event_group = NULL;
static const int ETH_CONNECTED_BIT = BIT0;
static const int ETH_GOT_IP_BIT = BIT1;

/**
 * @brief Initialize Ethernet driver with generic PHY (all IEEE 802.3 compliant PHYs)
 */
static esp_err_t eth_init(esp_eth_handle_t *eth_handle_out)
{
    if (eth_handle_out == NULL) {
        ESP_LOGE(TAG, "invalid argument: eth_handle_out cannot be NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Init common MAC and PHY configs to default */
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    /* Update PHY config based on board specific configuration */
    phy_config.phy_addr = CONFIG_EXAMPLE_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_EXAMPLE_ETH_PHY_RST_GPIO;
#if CONFIG_EXAMPLE_ETH_PHY_RST_TIMING_EN
    phy_config.hw_reset_assert_time_us = CONFIG_EXAMPLE_ETH_PHY_RST_ASSERT_TIME_US;
    phy_config.post_hw_reset_delay_ms = CONFIG_EXAMPLE_ETH_PHY_RST_DELAY_MS;
#endif /* CONFIG_EXAMPLE_ETH_PHY_RST_TIMING_EN */

    /* Init vendor specific MAC config to default */
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    /* Update vendor specific MAC config based on board configuration */
    esp32_emac_config.smi_gpio.mdc_num = CONFIG_EXAMPLE_ETH_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = CONFIG_EXAMPLE_ETH_MDIO_GPIO;

#if CONFIG_EXAMPLE_ETH_PHY_INTERFACE_RMII
    /* Configure RMII based on Kconfig when non-default configuration selected */
    esp32_emac_config.interface = EMAC_DATA_INTERFACE_RMII;

    /* Configure RMII clock mode and GPIO */
#if CONFIG_EXAMPLE_ETH_RMII_CLK_INPUT
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
#else /* CONFIG_EXAMPLE_ETH_RMII_CLK_OUTPUT */
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
#endif
    esp32_emac_config.clock_config.rmii.clock_gpio = CONFIG_EXAMPLE_ETH_RMII_CLK_GPIO;

#if CONFIG_EXAMPLE_ETH_RMII_CLK_EXT_LOOPBACK_EN
    esp32_emac_config.clock_config_out_in.rmii.clock_gpio = CONFIG_EXAMPLE_ETH_RMII_CLK_EXT_LOOPBACK_IN_GPIO;
    esp32_emac_config.clock_config_out_in.rmii.clock_mode = EMAC_CLK_EXT_IN;
#endif

#if SOC_EMAC_USE_MULTI_IO_MUX
    /* Configure RMII datapane GPIOs */
    esp32_emac_config.emac_dataif_gpio.rmii.tx_en_num = CONFIG_EXAMPLE_ETH_RMII_TX_EN_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.txd0_num = CONFIG_EXAMPLE_ETH_RMII_TXD0_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.txd1_num = CONFIG_EXAMPLE_ETH_RMII_TXD1_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.crs_dv_num = CONFIG_EXAMPLE_ETH_RMII_CRS_DV_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.rxd0_num = CONFIG_EXAMPLE_ETH_RMII_RXD0_GPIO;
    esp32_emac_config.emac_dataif_gpio.rmii.rxd1_num = CONFIG_EXAMPLE_ETH_RMII_RXD1_GPIO;
#endif /* SOC_EMAC_USE_MULTI_IO_MUX */
#endif /* CONFIG_EXAMPLE_ETH_PHY_INTERFACE_RMII */

    /* Create new ESP32 Ethernet MAC instance */
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    if (mac == NULL) {
        ESP_LOGE(TAG, "create MAC instance failed");
        return ESP_FAIL;
    }

    /* Create new generic PHY instance */
    esp_eth_phy_t *phy = esp_eth_phy_new_generic(&phy_config);
    if (phy == NULL) {
        ESP_LOGE(TAG, "create PHY instance failed");
        mac->del(mac);
        return ESP_FAIL;
    }

    /* Init Ethernet driver to default and install it */
    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    if (esp_eth_driver_install(&config, &eth_handle) != ESP_OK) {
        ESP_LOGE(TAG, "Ethernet driver install failed");
        mac->del(mac);
        phy->del(phy);
        return ESP_FAIL;
    }

    *eth_handle_out = eth_handle;

    return ESP_OK;
}

/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
        ESP_LOGI(TAG, "Ethernet Link Up");
        ESP_LOGI(TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        if (s_eth_event_group) {
            xEventGroupSetBits(s_eth_event_group, ETH_CONNECTED_BIT);
        }
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Ethernet Link Down");
        if (s_eth_event_group) {
            xEventGroupClearBits(s_eth_event_group, ETH_CONNECTED_BIT | ETH_GOT_IP_BIT);
        }
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "Ethernet Started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "Ethernet Stopped");
        break;
    default:
        break;
    }
}

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(TAG, "Ethernet Got IP Address");
    ESP_LOGI(TAG, "~~~~~~~~~~~");
    ESP_LOGI(TAG, "ETHIP:" IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(TAG, "ETHGW:" IPSTR, IP2STR(&ip_info->gw));
    ESP_LOGI(TAG, "~~~~~~~~~~~");

    if (s_eth_event_group) {
        xEventGroupSetBits(s_eth_event_group, ETH_GOT_IP_BIT);
    }
}

esp_err_t app_ethernet_init(void)
{
    /* Initialize Ethernet driver */
    esp_err_t ret = eth_init(&s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Ethernet driver");
        return ret;
    }

    /* Initialize TCP/IP network interface (should be called only once in application) */
    ESP_ERROR_CHECK(esp_netif_init());

    /* Create default event loop that running in background
     * Note: If Wi-Fi is enabled, app_network_init() already creates the default event loop,
     * so we check if it already exists to avoid ESP_ERR_INVALID_STATE error.
     */
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }

    /* Create instance of esp-netif for Ethernet */
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    s_eth_netif = esp_netif_new(&cfg);
    if (s_eth_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create Ethernet netif");
        return ESP_FAIL;
    }

    s_eth_netif_glue = esp_eth_new_netif_glue(s_eth_handle);
    if (s_eth_netif_glue == NULL) {
        ESP_LOGE(TAG, "Failed to create Ethernet netif glue");
        esp_netif_destroy(s_eth_netif);
        return ESP_FAIL;
    }

    /* Attach Ethernet driver to TCP/IP stack */
    ret = esp_netif_attach(s_eth_netif, s_eth_netif_glue);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach Ethernet to netif");
        esp_eth_del_netif_glue(s_eth_netif_glue);
        esp_netif_destroy(s_eth_netif);
        return ret;
    }

    /* Register user defined event handlers */
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));

    /* Create event group */
    s_eth_event_group = xEventGroupCreate();
    if (s_eth_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Ethernet initialized");
    return ESP_OK;
}

esp_err_t app_ethernet_start(void)
{
    if (s_eth_handle == NULL) {
        ESP_LOGE(TAG, "Ethernet not initialized. Call app_ethernet_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    /* Start Ethernet driver state machine */
    esp_err_t ret = esp_eth_start(s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Ethernet driver");
        return ret;
    }

    ESP_LOGI(TAG, "Waiting for Ethernet connection...");

    /* Wait for Ethernet connection and IP address */
    EventBits_t bits = xEventGroupWaitBits(s_eth_event_group,
                                           ETH_CONNECTED_BIT | ETH_GOT_IP_BIT,
                                           false,
                                           true,
                                           portMAX_DELAY);

    if ((bits & (ETH_CONNECTED_BIT | ETH_GOT_IP_BIT)) == (ETH_CONNECTED_BIT | ETH_GOT_IP_BIT)) {
        ESP_LOGI(TAG, "Ethernet connected and IP obtained");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Ethernet connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

esp_err_t app_ethernet_stop(void)
{
    if (s_eth_handle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Stop Ethernet driver state machine */
    esp_err_t ret = esp_eth_stop(s_eth_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop Ethernet driver");
        return ret;
    }

    /* Unregister event handlers */
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, got_ip_event_handler);
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event_handler);

    /* Delete netif glue and netif */
    if (s_eth_netif_glue) {
        esp_eth_del_netif_glue(s_eth_netif_glue);
        s_eth_netif_glue = NULL;
    }
    if (s_eth_netif) {
        esp_netif_destroy(s_eth_netif);
        s_eth_netif = NULL;
    }

    /* Delete event group */
    if (s_eth_event_group) {
        vEventGroupDelete(s_eth_event_group);
        s_eth_event_group = NULL;
    }

    ESP_LOGI(TAG, "Ethernet stopped");
    return ESP_OK;
}

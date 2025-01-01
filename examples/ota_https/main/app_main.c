/* OTA HTTPS Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_event.h>

#include <app_network.h>
#include <esp_log.h>
#include <esp_rmaker_factory.h>
#include <esp_rmaker_ota_https.h>
#include <app_reset.h>
#include <app_insights.h>

static const char *TAG = "app_mmain";

#define WIFI_SSID CONFIG_EXAMPLE_WIFI_SSID
#define WIFI_PASSWORD CONFIG_EXAMPLE_WIFI_PASSWORD
#define WIFI_RESET_BUTTON_TIMEOUT 3

static EventGroupHandle_t g_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "WiFi disconnected. Trying to connect.");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "WiFi connected, got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init_connect(void)
{
    g_wifi_event_group = xEventGroupCreate();

    esp_err_t err;
    err = esp_event_loop_create_default();
    if (err == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "Event loop creation failed. Continuing since it might be created elsewhere");
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP Event Loop creation failed.");
        return ESP_FAIL;
    };
    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &event_handler,
                                               NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &event_handler,
                                               NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD},
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    xEventGroupWaitBits(g_wifi_event_group,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdFALSE,
                        portMAX_DELAY);

    return ESP_OK;
}

void app_main()
{
    esp_err_t err = esp_event_loop_create_default();
    ESP_ERROR_CHECK(err);

    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Initialize factory parition for HTTPS OTA */
    err = esp_rmaker_factory_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize rmaker factory partition.");
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(wifi_init_connect());

    /* Needs to be done after WiFi is connected. */
    esp_rmaker_ota_https_enable(NULL);
    esp_rmaker_ota_https_fetch();
}

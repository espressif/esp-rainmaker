#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_random.h>
#include <unity.h>
#include "memory_checks.h"

#include <esp_rmaker_console.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_console.h>
#include <esp_rmaker_scenes.h>

#include <network_provisioning/manager.h>

#include "app_network.h"
#include "app_insights.h"

esp_rmaker_device_t *light_device;

static const char * TAG = "test_rmaker";
bool config_done = false;
static const int repeat = 1;

#define DEFAULT_POWER       true
#define DEFAULT_HUE         180
#define DEFAULT_SATURATION  100
#define DEFAULT_BRIGHTNESS  25

static void init_nvs_flash(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_LOGI(TAG, "nvs_flash_init");
    TEST_ASSERT(err == ESP_OK);
}

static void test_config(void)
{
    if (!config_done) {
        init_nvs_flash();
        app_network_init();
        test_utils_record_free_mem(); 
        config_done = true;
    }
}

/* Callback to handle param updates received from the RainMaker cloud */
static esp_err_t bulk_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_write_req_t write_req[],
        uint8_t count, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    ESP_LOGI(TAG, "Light received %d params in write", count);
    // We will return ESP_OK as testing callbacks is currently out of scope of this test 
    return ESP_OK;
}

TEST_CASE("nvs init deinit", "[rmaker]")
{
    init_nvs_flash();
    nvs_flash_deinit();
}

TEST_CASE("node init", "[rmaker]")
{
    test_config();
    for (int i = 0; i < repeat; i++) {
        esp_rmaker_config_t rainmaker_cfg = {
            .enable_time_sync = false,
        };
        esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Lightbulb");
        TEST_ASSERT(node != NULL);

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        TEST_ASSERT(esp_rmaker_node_deinit(node) == ESP_OK);
    }
}

TEST_CASE("add device", "[rmaker]")
{
    test_config();
    for (int i = 0; i < repeat; i++) {
        esp_rmaker_config_t rainmaker_cfg = {
            .enable_time_sync = false,
        };
        esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Lightbulb");
        TEST_ASSERT(node != NULL);

        light_device = esp_rmaker_lightbulb_device_create("Light", NULL, DEFAULT_POWER);
        TEST_ASSERT(light_device != NULL);

        esp_rmaker_device_add_param(light_device, esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, DEFAULT_BRIGHTNESS));

        int ret = esp_rmaker_node_add_device(node, light_device);

        TEST_ASSERT(ret == ESP_OK);

        vTaskDelay(1000 / portTICK_PERIOD_MS);

        TEST_ASSERT(esp_rmaker_node_deinit(node) == ESP_OK);
    }
}

TEST_CASE("rmaker start stop", "[rmaker]")
{
    test_config();
    for (int i = 0; i < repeat; i++) {
        esp_rmaker_config_t rainmaker_cfg = {
            .enable_time_sync = false,
        };
        esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Lightbulb");
        TEST_ASSERT(node != NULL);

        light_device = esp_rmaker_lightbulb_device_create("Light", NULL, DEFAULT_POWER);
        TEST_ASSERT(light_device != NULL);
        
        TEST_ASSERT(esp_rmaker_device_add_bulk_cb(light_device, bulk_write_cb, NULL) == ESP_OK);

        esp_rmaker_device_add_param(light_device, esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, DEFAULT_BRIGHTNESS));
        esp_rmaker_device_add_param(light_device, esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, DEFAULT_HUE));
        esp_rmaker_device_add_param(light_device, esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME, DEFAULT_SATURATION));
        
        int ret = esp_rmaker_node_add_device(node, light_device);
        TEST_ASSERT(ret == ESP_OK);

        TEST_ASSERT(esp_rmaker_start() == ESP_OK);

        TEST_ASSERT(app_network_start(POP_TYPE_RANDOM) == ESP_OK);

        ESP_LOGD(TAG, "Will wait for 1 seconds before calling esp_rmaker_stop");
        vTaskDelay(5*1000 / portTICK_PERIOD_MS);

        TEST_ASSERT(esp_rmaker_stop() == ESP_OK);
        TEST_ASSERT(esp_rmaker_node_deinit(node) == ESP_OK);
    }
}

/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <app_insights.h>
#include <app_reset.h>
#include <esp_rmaker_console.h>

#include "app_priv.h"
#include <app_matter.h>
#include "app_matter_light.h"
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
#include <platform/ThreadStackManager.h>
#include <platform/ESP32/OpenthreadLauncher.h>
#endif
#ifndef CONFIG_EXAMPLE_USE_RAINMAKER_FABRIC
#include <app_network.h>
#include <esp_rmaker_user_mapping.h>
#include <network_provisioning/manager.h>
#include <matter_commissioning_window_management.h>
#if CONFIG_DYNAMIC_PASSCODE_COMMISSIONABLE_DATA_PROVIDER
#include <esp_matter_providers.h>
#include <dynamic_commissionable_data_provider.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/OnboardingCodesUtil.h>
dynamic_commissionable_data_provider g_dynamic_passcode_provider;
#endif
#endif

static const char *TAG = "app_main";

static app_driver_handle_t light_handle;

bool rmaker_init_done = false; // used with extern in `app_matter.c`

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t bulk_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_write_req_t write_req[],
    uint8_t count, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }

    ESP_LOGI(TAG, "Light received %d params in write", count);
    for (int i = 0; i < count; i++) {
        const esp_rmaker_param_t *param = write_req[i].param;
        esp_rmaker_param_val_t val = write_req[i].val;
        const char *param_name = esp_rmaker_param_get_name(param);
    
        if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0) {
            app_driver_light_set_power(light_handle, val.val.b);
            app_matter_report_power(val.val.b);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_HUE_NAME) == 0) {
            app_driver_light_set_hue(light_handle, val.val.i);
            app_matter_report_hue(val.val.i);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_SATURATION_NAME) == 0) {
            app_driver_light_set_saturation(light_handle, val.val.i);
            app_matter_report_saturation(val.val.i);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_CCT_NAME) == 0) {
            app_driver_light_set_temperature(light_handle, val.val.i);
            app_matter_report_temperature(val.val.i);
        } else if (strcmp(param_name, ESP_RMAKER_DEF_BRIGHTNESS_NAME) == 0) {
            app_driver_light_set_brightness(light_handle, val.val.i);
            app_matter_report_brightness(val.val.i);
        }
        esp_rmaker_param_update(param, val);
    }
    return ESP_OK;
}

extern "C" void app_main()
{
    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize drivers for light and button */
    light_handle = app_driver_light_init();
    app_driver_handle_t button_handle = app_driver_button_init(light_handle);
    app_reset_button_register(button_handle);

    /* Initialize Matter */
    app_matter_init(app_attribute_update_cb,app_identification_cb);
#ifdef CONFIG_EXAMPLE_USE_RAINMAKER_FABRIC
    app_matter_rmaker_init();
#else
    app_network_init();
#endif

    /* Create Data Model for esp-matter */
    app_matter_light_create(light_handle);

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
   esp_openthread_platform_config_t ot_config = {
        .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
        .port_config = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
    };
    set_openthread_platform_config(&ot_config);
    // This will not really initiaize Thread stack as the thread stack has been initialzed in app_network.
    // We call this function to pass the OpenThread instance to GenericThreadStackManagerImpl_OpenThread
    // so that it can be used for SRP service registration and network commissioning driver.
    chip::DeviceLayer::ThreadStackMgr().InitThreadStack();
#endif

    /* Starting driver with default values */
    app_driver_light_set_defaults();

#ifdef CONFIG_EXAMPLE_USE_RAINMAKER_FABRIC
    /* Matter start */
    app_matter_start(app_event_cb);
#endif

    /* Create Data Model for esp-matter */
    /* Initialize the ESP RainMaker Agent.
     * Create Lightbulb device and its parameters.
    */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Device", "Lightbulb");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node.");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    esp_rmaker_device_t *light_device = esp_rmaker_lightbulb_device_create(LIGHT_DEVICE_NAME, NULL, DEFAULT_POWER);
    esp_rmaker_device_add_bulk_cb(light_device, bulk_write_cb, NULL);

    esp_rmaker_device_add_param(light_device, esp_rmaker_brightness_param_create(ESP_RMAKER_DEF_BRIGHTNESS_NAME, DEFAULT_BRIGHTNESS));
    esp_rmaker_device_add_param(light_device, esp_rmaker_saturation_param_create(ESP_RMAKER_DEF_SATURATION_NAME, DEFAULT_SATURATION));
    esp_rmaker_device_add_param(light_device, esp_rmaker_hue_param_create(ESP_RMAKER_DEF_HUE_NAME, DEFAULT_HUE));
    esp_rmaker_device_add_param(light_device, esp_rmaker_cct_param_create(ESP_RMAKER_DEF_CCT_NAME, DEFAULT_TEMPERATURE));

    esp_rmaker_node_add_device(node, light_device);

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

    /* Enable factory service */
    esp_rmaker_system_serv_config_t servconfig = {
        .flags = SYSTEM_SERV_FLAGS_ALL,
        .reboot_seconds = 2,
        .reset_reboot_seconds = 2,
    };
    esp_rmaker_system_service_enable(&servconfig);

#ifdef CONFIG_EXAMPLE_USE_RAINMAKER_FABRIC
    /* Pre start */
    ESP_ERROR_CHECK(app_matter_rmaker_start());
#else
    err = app_network_set_custom_mfg_data(MGF_DATA_DEVICE_TYPE_LIGHT, MFG_DATA_DEVICE_SUBTYPE_LIGHT);

    err = app_network_start(POP_TYPE_MAC);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        abort();
    }
    matter_commissioning_window_management_enable();
#if CONFIG_DYNAMIC_PASSCODE_COMMISSIONABLE_DATA_PROVIDER
    /* This should be called before esp_matter::start() */
    esp_matter::set_custom_commissionable_data_provider(&g_dynamic_passcode_provider);
#endif
    /* Matter start */
    app_matter_start(app_event_cb);

    bool is_network_provisioned = false;
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
    // If Wi-Fi is provisioned and RainMaker user node mapping is done, deinitialize the BLE.
    network_prov_mgr_is_wifi_provisioned(&is_network_provisioned);
#else
    network_prov_mgr_is_thread_provisioned(&is_network_provisioned);
#endif
    if (is_network_provisioned && esp_rmaker_user_node_mapping_get_state() == ESP_RMAKER_USER_MAPPING_DONE) {
        chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t) { chip::DeviceLayer::Internal::BLEMgr().Shutdown(); });
    }
#endif
    
    rmaker_init_done = true;

    app_matter_enable_matter_console();

    // RainMaker start is deferred after Matter commissioning is complete
    // and BLE memory is reclaimed, so that MQTT connect doesnt fail.
}

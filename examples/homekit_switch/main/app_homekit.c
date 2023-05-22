/* HomeKit Switch control
   
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>

#include <hap.h>
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>

#include <qrcode.h>

#include "app_priv.h"

static const char *TAG = "app_homekit";

static hap_char_t *on_char;

static void app_homekit_show_qr(void)
{
#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
#define QRCODE_BASE_URL     "https://espressif.github.io/esp-homekit-sdk/qrcode.html"
    char *setup_payload =  esp_hap_get_setup_payload(CONFIG_EXAMPLE_SETUP_CODE,
            CONFIG_EXAMPLE_SETUP_ID, false, HAP_CID_SWITCH);
    if (setup_payload) {
        printf("-----QR Code for HomeKit-----\n");
        printf("Scan this QR code from the Home app on iOS\n");
        qrcode_display(setup_payload);
        printf("If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s\n",
                QRCODE_BASE_URL, setup_payload);
        free(setup_payload);
    }
#else
    ESP_LOGW(TAG, "Cannot show QR code for HomeKit pairing as the raw setup code is not available.");
#endif
}
static void app_homekit_event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (hap_get_paired_controller_count() == 0) {
            app_homekit_show_qr();
        } else {
            ESP_LOGI(TAG, "Accessory is already paired with a controller.");
        }
    } else if (event_base == HAP_EVENT && event_id == HAP_EVENT_CTRL_UNPAIRED) {
        if (hap_get_paired_controller_count() == 0) {
            app_homekit_show_qr();
        }
    }
}
/* Mandatory identify routine for the accessory.
 * In a real accessory, something like LED blink should be implemented
 * got visual identification
 */
static int switch_identify(hap_acc_t *ha)
{
    bool cur_state = app_driver_get_state();
    app_indicator_set(!cur_state);
    vTaskDelay(500/portTICK_PERIOD_MS);
    app_indicator_set(cur_state);
    vTaskDelay(500/portTICK_PERIOD_MS);
    app_indicator_set(!cur_state);
    vTaskDelay(500/portTICK_PERIOD_MS);
    app_indicator_set(cur_state);
    ESP_LOGI(TAG, "Accessory identified");
    return HAP_SUCCESS;
}

static int switch_write(hap_write_data_t write_data[], int count,
        void *serv_priv, void *write_priv)
{
    int i, ret = HAP_SUCCESS;
    hap_write_data_t *write;
    for (i = 0; i < count; i++) {
        write = &write_data[i];
        if (!strcmp(hap_char_get_type_uuid(write->hc), HAP_CHAR_UUID_ON)) {
            ESP_LOGI(TAG, "Received Write. Switch %s", write->val.b ? "On" : "Off");
            /* Set the switch state */
            app_driver_set_state(write->val.b);
            /* Update the HomeKit characteristic */
            hap_char_update_val(write->hc, &(write->val));
            /* Report to RainMaker */
            esp_rmaker_param_update_and_report(
                esp_rmaker_device_get_param_by_name(switch_device, ESP_RMAKER_DEF_POWER_NAME),
                esp_rmaker_bool(write->val.b));

            *(write->status) = HAP_STATUS_SUCCESS;
        } else {
            *(write->status) = HAP_STATUS_RES_ABSENT;
        }
    }
    return ret;
}
esp_err_t app_homekit_update_state(bool state)
{
    hap_val_t new_value = {
        .b = state,
    };

    hap_char_update_val(on_char, &new_value);
    return ESP_OK;
}
esp_err_t app_homekit_start(bool init_state)
{
    hap_acc_t *accessory;
    hap_serv_t *service;

    /* Initialize the HAP core */
    hap_init(HAP_TRANSPORT_WIFI);

    /* Initialise the mandatory parameters for Accessory which will be added as
     * the mandatory services internally
     */
    hap_acc_cfg_t cfg = {
        .name = "Esp RainMaker Device",
        .manufacturer = "Espressif",
        .model = "homekit_switch",
        .serial_num = "001122334455",
        .fw_rev = "1.0",
        .hw_rev = NULL,
        .pv = "1.1.0",
        .identify_routine = switch_identify,
        .cid = HAP_CID_SWITCH,
    };
    /* Create accessory object */
    accessory = hap_acc_create(&cfg);

    /* Create the Outlet Service. Include the "name" since this is a user visible service  */
    service = hap_serv_switch_create(init_state);
    hap_serv_add_char(service, hap_char_name_create("Switch"));

    /* Set the write callback for the service */
    hap_serv_set_write_cb(service, switch_write);

    /* Get pointer to the on_char to be used during update */
    on_char = hap_serv_get_char_by_uuid(service, HAP_CHAR_UUID_ON);

    /* Add the Outlet Service to the Accessory Object */
    hap_acc_add_serv(accessory, service);

    /* Add the Accessory to the HomeKit Database */
    hap_add_accessory(accessory);

    /* For production accessories, the setup code shouldn't be programmed on to
     * the device. Instead, the setup info, derived from the setup code must
     * be used. Use the factory_nvs_gen utility to generate this data and then
     * flash it into the factory NVS partition.
     *
     * By default, the setup ID and setup info will be read from the factory_nvs
     * Flash partition and so, is not required to set here explicitly.
     *
     * However, for testing purpose, this can be overridden by using hap_set_setup_code()
     * and hap_set_setup_id() APIs, as has been done here.
     */
#ifdef CONFIG_EXAMPLE_USE_HARDCODED_SETUP_CODE
    /* Unique Setup code of the format xxx-xx-xxx. Default: 111-22-333 */
    hap_set_setup_code(CONFIG_EXAMPLE_SETUP_CODE);
    /* Unique four character Setup Id. Default: ES32 */
    hap_set_setup_id(CONFIG_EXAMPLE_SETUP_ID);
    if (hap_get_paired_controller_count() == 0) {
        app_homekit_show_qr();
    }
#endif
    hap_enable_mfi_auth(HAP_MFI_AUTH_HW);
    /* Register our event handler for Wi-Fi, IP and Provisioning related events */
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &app_homekit_event_handler, NULL);
    esp_event_handler_register(HAP_EVENT, HAP_EVENT_CTRL_UNPAIRED, &app_homekit_event_handler, NULL);

    /* After all the initializations are done, start the HAP core */
    if (hap_start() == 0) {
        ESP_LOGI(TAG, "HomeKit started successfully");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Failed to start HomeKit");
    return ESP_FAIL;

}

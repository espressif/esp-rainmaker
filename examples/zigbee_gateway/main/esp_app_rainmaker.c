/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_check.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <app_network.h>
#include <app_insights.h>
#include <esp_app_rainmaker.h>
#include <iot_button.h>
#include <esp_rmaker_utils.h>
#include "esp_timer.h"
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include <esp_rmaker_standard_devices.h>
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_scenes.h>
#include <esp_gateway_control.h>
#include "esp_gateway_main.h"
#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
#include "m5display.h"
#endif

/* Rainmaker app driver: button factory reset */
/* This is the button that is used for toggling the power */
#define BUTTON_GPIO          0 // GPIO0, esp32s3
#define REBOOT_DELAY        2
#define RESET_DELAY         2
#define ZIGBEE_GATEWAY_INFO_PART_NAME "nvs"
#define ZIGBEE_GATEWAY_NAMESPACE "zggw"
#define ZIGBEE_GATEWAY_DEVICE_ENDPOINT_KEY "ep_%d"
#define ZIGBEE_GATEWAY_DEVICE_TYPE_KEY "type_%d"
#define ZIGBEE_GATEWAY_DEVICE_ADDRS_KEY "addr"
#define ZIGBEE_MAX_DEVICE_COUNT 16
static uint8_t device_count = 0;
static const char *TAG = "esp_app_rainmaker";
static uint16_t device_addrs[ZIGBEE_MAX_DEVICE_COUNT];
static bool perform_factory_reset = false;
esp_rmaker_device_t *zigbee_gw_device;
esp_rainmaker_gateway_joining_device_list_t *end_device_list = NULL;

uint8_t get_zigbee_device_count()
{
    return device_count;
}

static esp_err_t app_gateway_store_device_info(uint16_t short_addr, uint8_t endpoint, uint16_t device_type)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    err = nvs_open_from_partition(ZIGBEE_GATEWAY_INFO_PART_NAME, ZIGBEE_GATEWAY_NAMESPACE, NVS_READWRITE,
                                  &handle);
    ESP_RETURN_ON_ERROR(err, TAG, "Error opening partition %s namespace %s. Err: %d", ZIGBEE_GATEWAY_INFO_PART_NAME, ZIGBEE_GATEWAY_NAMESPACE, err);
    char device_key[16] = { 0 };
    snprintf(device_key, sizeof(device_key), ZIGBEE_GATEWAY_DEVICE_ENDPOINT_KEY, short_addr);
    err = nvs_set_blob(handle, device_key, &endpoint, sizeof(endpoint));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error storing the device endpoint");
    }

    snprintf(device_key, sizeof(device_key), ZIGBEE_GATEWAY_DEVICE_TYPE_KEY, short_addr);
    err = nvs_set_blob(handle, device_key, &device_type, sizeof(device_type));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error storing the device type");
    }
    nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t app_gateway_remove_device_info(uint16_t short_addr)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    err = nvs_open_from_partition(ZIGBEE_GATEWAY_INFO_PART_NAME, ZIGBEE_GATEWAY_NAMESPACE, NVS_READWRITE,
                                  &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening partition %s namespace %s. Err: %d", ZIGBEE_GATEWAY_INFO_PART_NAME,
                 ZIGBEE_GATEWAY_NAMESPACE, err);
        return err;
    }
    char device_key[16] = { 0 };
    snprintf(device_key, sizeof(device_key), ZIGBEE_GATEWAY_DEVICE_ENDPOINT_KEY, short_addr);
    err = nvs_erase_key(handle, device_key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error erasing the device endpoint");
    }

    snprintf(device_key, sizeof(device_key), ZIGBEE_GATEWAY_DEVICE_TYPE_KEY, short_addr);
    err = nvs_erase_key(handle, device_key);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error erasing the device type");
    }

    nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t app_gateway_read_device_info(uint16_t short_addr, uint8_t *endpoint, uint16_t *device_type)
{
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    err = nvs_open_from_partition(ZIGBEE_GATEWAY_INFO_PART_NAME, ZIGBEE_GATEWAY_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening partition %s namespace %s. Err: %d", ZIGBEE_GATEWAY_INFO_PART_NAME,
                 ZIGBEE_GATEWAY_NAMESPACE, err);
        return err;
    }
    size_t len = sizeof(*endpoint);
    char device_key[16] = { 0 };
    snprintf(device_key, sizeof(device_key), ZIGBEE_GATEWAY_DEVICE_ENDPOINT_KEY, short_addr);
    err = nvs_get_blob(handle, device_key, endpoint, &len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading the device endpoint");
        goto close;
    }

    len = sizeof(*device_type);
    snprintf(device_key, sizeof(device_key), ZIGBEE_GATEWAY_DEVICE_TYPE_KEY, short_addr);
    err = nvs_get_blob(handle, device_key, device_type, &len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading the device type");
    }

close:
    nvs_close(handle);
    return err;
}

static esp_err_t app_gateway_store_device_addrs()
{
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    err = nvs_open_from_partition(ZIGBEE_GATEWAY_INFO_PART_NAME, ZIGBEE_GATEWAY_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening partition %s namespace %s. Err: %d", ZIGBEE_GATEWAY_INFO_PART_NAME,
                 ZIGBEE_GATEWAY_NAMESPACE, err);
        return err;
    }

    err = nvs_set_blob(handle, ZIGBEE_GATEWAY_DEVICE_ADDRS_KEY, device_addrs, sizeof(device_addrs));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error storing the device count");
    }
    nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t app_gateway_read_device_addrs()
{
    esp_err_t err = ESP_OK;
    nvs_handle_t handle;
    err = nvs_open_from_partition(ZIGBEE_GATEWAY_INFO_PART_NAME, ZIGBEE_GATEWAY_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening partition %s namespace %s. Err: %d", ZIGBEE_GATEWAY_INFO_PART_NAME,
                 ZIGBEE_GATEWAY_NAMESPACE, err);
        return err;
    }
    size_t len = sizeof(device_addrs);
    err = nvs_get_blob(handle, ZIGBEE_GATEWAY_DEVICE_ADDRS_KEY, device_addrs, &len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading the device count");
    }
    nvs_close(handle);
    return err;
}

#if !CONFIG_ZIGBEE_INSTALLCODE_ENABLED
static esp_timer_handle_t zigbee_network_open_timer_handle = NULL;
static uint64_t zigbee_network_open_timeout_period_us = ESP_ZIGBEE_GATWAY_OPEN_NETWORK_DEFAULT_TIME * 1000* 1000;
static void esp_rainmaker_update_zigbee_add_device_state(bool new_state);

static void zigbee_open_network_timer_stop(void *time)
{
    esp_rainmaker_update_zigbee_add_device_state(0);
    ESP_LOGI(TAG, "Zigbe gateway network closed");
    esp_timer_stop(zigbee_network_open_timer_handle);
    esp_timer_delete(zigbee_network_open_timer_handle);
    zigbee_network_open_timer_handle = NULL;
#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
    bool flag = false;
    esp_event_post(M5STACK_DISPLAY_EVENT, DISPLAY_ZIGBEE_ADD_DEVICE_STATE, &flag, sizeof(flag), portMAX_DELAY);
#endif
}

static esp_err_t zigbee_networtk_close_start_timer(void)
{
    esp_timer_create_args_t zigbee_open_network_timer_conf = {
        .callback = zigbee_open_network_timer_stop,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "zigbee_networtk_close"
    };
    if (esp_timer_create(&zigbee_open_network_timer_conf, &zigbee_network_open_timer_handle) == ESP_OK) {
        esp_timer_start_once(zigbee_network_open_timer_handle, zigbee_network_open_timeout_period_us);
        ESP_LOGI(TAG, "Zigbee network close after %d seconds", ESP_ZIGBEE_GATWAY_OPEN_NETWORK_DEFAULT_TIME);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to create zigbee network open timer");
    }
    return ESP_FAIL;
}

static void esp_rainmaker_update_zigbee_add_device_state(bool new_state)
{
    esp_rmaker_param_update_and_notify(
        esp_rmaker_device_get_param_by_name(zigbee_gw_device, ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE),
        esp_rmaker_bool(new_state));
}

void esp_rainmaker_report_zigbee_add_device_state(bool new_state)
{
    esp_rmaker_param_update_and_report(
        esp_rmaker_device_get_param_by_name(zigbee_gw_device, ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE),
        esp_rmaker_bool(new_state));
}

void esp_app_enable_zigbee_add_device()
{
    ESP_LOGI(TAG, "Zigbee gateway open network for %d second", ESP_ZIGBEE_GATWAY_OPEN_NETWORK_DEFAULT_TIME);
    esp_gateway_control_permit_join(ESP_ZIGBEE_GATWAY_OPEN_NETWORK_DEFAULT_TIME);
    if (zigbee_network_open_timer_handle == NULL) {
        zigbee_networtk_close_start_timer();
    } else {
        ESP_LOGI(TAG, "Failed to start zigbee network open timer");
    }
}

void esp_app_disable_zigbee_add_device()
{
    esp_gateway_control_permit_join(0); // permit_duration = 0 indicate that permission is disabled
    ESP_LOGI(TAG, "Zigbee gateway network closed");
    if (zigbee_network_open_timer_handle) {
        esp_timer_stop(zigbee_network_open_timer_handle);
        esp_timer_delete(zigbee_network_open_timer_handle);
        zigbee_network_open_timer_handle = NULL;
    }
}
#endif

esp_rmaker_device_t *esp_rmaker_zigbee_gateway_device_create(const char *dev_name,
        void *priv_data)
{
    esp_rmaker_device_t *device = esp_rmaker_device_create(dev_name, ESP_RMAKER_DEVICE_ZIGBEE_GATEWAY, priv_data);
    if (device) {
        esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, dev_name));
#if CONFIG_ZIGBEE_INSTALLCODE_ENABLED
        esp_rmaker_param_t *param = esp_rmaker_param_create(ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE, ESP_RMAKER_PARAM_ADD_ZIGBEE_DEVICE, esp_rmaker_str(""), PROP_FLAG_READ | PROP_FLAG_WRITE);
        esp_rmaker_param_add_ui_type(param, ESP_RMAKER_UI_QR_SCAN);
#else 
        esp_rmaker_param_t *param = esp_rmaker_param_create(ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE, ESP_RMAKER_PARAM_ADD_ZIGBEE_DEVICE, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
        esp_rmaker_param_add_ui_type(param, ESP_RMAKER_UI_TOGGLE);
#endif // CONFIG_ZIGBEE_INSTALLCODE_ENABLED
        esp_rmaker_device_add_param(device, param);
    }
    return device;
}

static void leave_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx)
{
    uint16_t *short_addr = (uint16_t *)user_ctx;
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Device(0x%4x) has left", *short_addr);
    }
    free(user_ctx);
}

/* Callback to handle commands received from the RainMaker cloud */
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    char *device_name = esp_rmaker_device_get_name(device);
    char *param_name = esp_rmaker_param_get_name(param);
    ESP_LOGI(TAG, "Received device name = %s,parameter name = %s", device_name, param_name);
    if (strcmp(param_name, ESP_RMAKER_DEF_ADD_ZIGBEE_DEVICE) == 0) {
#if CONFIG_ZIGBEE_INSTALLCODE_ENABLED
        esp_zigbee_ic_mac_address_t *IC_MacAddress = (esp_zigbee_ic_mac_address_t *)malloc(sizeof(esp_zigbee_ic_mac_address_t));
        if (esp_zb_prase_ic_obj(val.val.s, IC_MacAddress) == ESP_OK) {
            ESP_LOGI(TAG, "prase zigbee ic success");
            ESP_LOGI(TAG, "prase install code:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x CRC:%02x%02x",
                     IC_MacAddress->ic[0], IC_MacAddress->ic[1], IC_MacAddress->ic[2], IC_MacAddress->ic[3],
                     IC_MacAddress->ic[4], IC_MacAddress->ic[5], IC_MacAddress->ic[6], IC_MacAddress->ic[7],
                     IC_MacAddress->ic[8], IC_MacAddress->ic[9], IC_MacAddress->ic[10], IC_MacAddress->ic[11],
                     IC_MacAddress->ic[12], IC_MacAddress->ic[13], IC_MacAddress->ic[14], IC_MacAddress->ic[15],
                     IC_MacAddress->ic[16], IC_MacAddress->ic[17]);
            ESP_LOGI(TAG, "prase MAC address::%02x%02x%02x%02x%02x%02x%02x%02x",
                     IC_MacAddress->addr[7], IC_MacAddress->addr[6], IC_MacAddress->addr[5], IC_MacAddress->addr[4],
                     IC_MacAddress->addr[3], IC_MacAddress->addr[2], IC_MacAddress->addr[1], IC_MacAddress->addr[0]);
            /* Gateway could start permit device to join and add install code */
            esp_gateway_control_permit_join(ESP_ZIGBEE_GATWAY_OPEN_NETWORK_DEFAULT_TIME);
            esp_gateway_control_secur_ic_add(IC_MacAddress, ESP_ZB_IC_TYPE_128);
        } else {
            ESP_LOGE(TAG, "prase zigbee ic failed");
        }
 #else
        if (val.val.b == true) {
            esp_app_enable_zigbee_add_device();
        } else {
            esp_app_disable_zigbee_add_device();
        }
#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
        esp_event_post(M5STACK_DISPLAY_EVENT, DISPLAY_ZIGBEE_ADD_DEVICE_STATE, &val.val.b, sizeof(val.val.b), portMAX_DELAY);
#endif
#endif // CONFIG_ZIGBEE_INSTALLCODE_ENABLED
    }
    esp_rmaker_param_update_and_report(param, val);
    return ESP_OK;
}

static void esp_app_rainmaker_get_zigbee_nwk_info(const esp_rmaker_device_t *device, device_params_t *ret_info)
{
    uint32_t private_data = *(uint32_t *)esp_rmaker_device_get_priv_data(device);
    uint16_t short_address = (uint16_t)(private_data >> 16);
    uint8_t endpoint = (uint8_t)(private_data);
    ret_info->short_addr = short_address;
    ret_info->endpoint = endpoint;
}

static esp_err_t zigbee_device_write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                                        const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (ctx) {
        ESP_LOGI(TAG, "Received write request via : %s", esp_rmaker_device_cb_src_to_str(ctx->src));
    }
    if (strcmp(esp_rmaker_param_get_name(param), ESP_RMAKER_DEF_POWER_NAME) == 0) {
        ESP_LOGI(TAG, "Received value = %s for %s - %s",
                 val.val.b ? "true" : "false", esp_rmaker_device_get_name(device),
                 esp_rmaker_param_get_name(param));
        /* light on/off control with corresponding short address and endpoint */
        device_params_t device_info;
        esp_app_rainmaker_get_zigbee_nwk_info(device, &device_info);
        esp_gateway_control_light_on_off(val.val.b, &device_info);
        esp_rmaker_param_update_and_report(param, val);
    } else if (strcmp(esp_rmaker_param_get_name(param), "Delete device") == 0) {
        device_params_t device_info;
        esp_app_rainmaker_get_zigbee_nwk_info(device, &device_info);
        esp_zb_zdo_mgmt_leave_req_param_t cmd_req;
        esp_zb_ieee_addr_t ieee_addr;
        if (esp_zb_ieee_address_by_short(device_info.short_addr, ieee_addr) != ESP_OK) {
            return ESP_ERR_NOT_FOUND;
        }
        cmd_req.dst_nwk_addr = device_info.short_addr;
        memcpy(cmd_req.device_address, ieee_addr, sizeof(ieee_addr));
        cmd_req.rejoin = 0;
        cmd_req.remove_children = 1;
        uint16_t *short_addr = (uint16_t *)malloc(sizeof(uint16_t));
        *short_addr = device_info.short_addr;
        esp_zb_lock_acquire(portMAX_DELAY);
        esp_zb_zdo_device_leave_req(&cmd_req, leave_cb, short_addr);
        esp_zb_lock_release();

        esp_rainmaker_gateway_joining_device_list_t *curr = end_device_list;
        esp_rainmaker_gateway_joining_device_list_t *pre = NULL;
        while (curr) {
            if ((curr->zigbee_info.short_addr == device_info.short_addr) && (curr->zigbee_info.endpoint == device_info.endpoint)) {
                for (size_t index = 0; index < ZIGBEE_MAX_DEVICE_COUNT; ++index) {
                    if (device_addrs[index] == device_info.short_addr) {
                        device_addrs[index] = 0xfffe;
                        break;
                    }
                }
                --device_count;
                app_gateway_store_device_addrs();
                app_gateway_remove_device_info(device_info.short_addr);
                esp_rmaker_node_remove_device(esp_rmaker_get_node(), curr->device);
                esp_rmaker_device_delete(curr->device);
                esp_rmaker_report_node_details();
                if (pre) {
                    pre->next = curr->next;
                } else {
                    end_device_list = curr->next;
                }
                free(curr);
                ESP_LOGI(TAG, "remove device(0x%4x)", device_info.short_addr);
#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
                esp_event_post(M5STACK_DISPLAY_EVENT, DISPLAY_ZIGBEE_DEVICE_COUNT_EVENT, &device_count, sizeof(device_count), portMAX_DELAY);
#endif
                break;
            }
            pre = curr;
            curr = curr->next;
        }
    }
    return ESP_OK;
}

static esp_err_t esp_app_rainmaker_add_end_device(esp_zb_ha_standard_devices_t device_type, uint16_t short_address, uint8_t endpoint, bool storage)
{
    char device_name[50];
    uint32_t *private_data = (uint32_t *)malloc(sizeof(uint32_t)); /* need to free it if remove device */
    /* form a uin32 type private data as (XX,XX)(00,xx)*/
    *private_data = ((uint32_t)short_address << 16U) + endpoint;
    esp_rmaker_device_t *device = NULL;
    switch (device_type) {
    case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID:
        sprintf(device_name, "Light_0x%x_%d", short_address, endpoint);
        device = esp_rmaker_lightbulb_device_create(device_name, (void *)(private_data), false);
        break;
    case ESP_ZB_HA_IAS_ZONE_ID:
        sprintf(device_name, "Sensor_0x%x_%d", short_address, endpoint);
        device = esp_rmaker_device_create(device_name, "esp.device.contact-sensor", (void *)(private_data));
        if (device) {
            esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, device_name));
            esp_rmaker_param_t *binary_param = esp_rmaker_param_create("Door Status", ESP_RMAKER_PARAM_MODE, esp_rmaker_bool(false), PROP_FLAG_READ);
            esp_rmaker_param_add_ui_type(binary_param, ESP_RMAKER_UI_TOGGLE);
            esp_rmaker_device_add_param(device, binary_param);
        }
        break;
    default:
        break;
    }

    esp_rmaker_param_t *reset_param = esp_rmaker_param_create("Delete device", ESP_RMAKER_PARAM_MODE, esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    esp_rmaker_param_add_ui_type(reset_param, ESP_RMAKER_UI_TRIGGER);
    esp_rmaker_device_add_param(device, reset_param);

    esp_rainmaker_gateway_joining_device_list_t *new = (esp_rainmaker_gateway_joining_device_list_t *)malloc(sizeof(esp_rainmaker_gateway_joining_device_list_t));
    new->device = device;
    esp_rmaker_device_add_cb(new->device, zigbee_device_write_cb, NULL);
    esp_rmaker_node_add_device(esp_rmaker_get_node(), new->device);
    new->attribute_value = false;
    new->device_type = device_type;
    new->zigbee_info.endpoint = endpoint;
    new->zigbee_info.short_addr = short_address;
    new->next = end_device_list;
    end_device_list = new;

    if (storage) {
        // Don't storage device info when resuming devices.
        bool device_addrs_has_space = false;
        for (size_t i = 0; i < ZIGBEE_MAX_DEVICE_COUNT; ++i) {
            if (device_addrs[i] == 0xfffe) {
                device_addrs[i] = short_address;
                device_addrs_has_space = true;
                break;
            }
        }
        if (!device_addrs_has_space) {
            ESP_LOGE(TAG, "No enough space to store device info");
            return ESP_ERR_NO_MEM;
        }
        ++device_count;
        app_gateway_store_device_info(short_address, endpoint, device_type);
        app_gateway_store_device_addrs();
    }
    return ESP_OK;
}

static void resume_device()
{
    app_gateway_read_device_addrs();
    for (size_t index = 0; index < ZIGBEE_MAX_DEVICE_COUNT; ++index) {
        if (device_addrs[index] != 0xfffe) {
            uint8_t endpoint = 0;
            uint16_t type = 0;
            if (app_gateway_read_device_info(device_addrs[index], &endpoint, &type) == ESP_OK &&
                esp_app_rainmaker_add_end_device(type, device_addrs[index], endpoint, false) == ESP_OK) {
                ++device_count;
            }
        }
    }
    if (esp_rmaker_report_node_details() != ESP_OK) {
        ESP_LOGE(TAG, "Report node state failed.");
    }
}

esp_err_t esp_app_rainmaker_update_device_param(uint16_t short_address, uint8_t endpoint, bool value)
{
    esp_rainmaker_gateway_joining_device_list_t *curr = end_device_list;
    while (curr) {
        if ((curr->zigbee_info.short_addr == short_address) && (curr->zigbee_info.endpoint == endpoint)) {
            // update value
            curr->attribute_value = value;
            esp_rmaker_param_val_t val;
            val.type = RMAKER_VAL_TYPE_BOOLEAN;
            val.val.b = curr->attribute_value;
            esp_rmaker_param_update_and_report(esp_rmaker_device_get_param_by_name(curr->device, "Door Status"), val);
            break;
        }
        curr = curr->next;
    }
    return ESP_OK;
}

esp_err_t esp_app_rainmaker_add_joining_device(esp_zb_ha_standard_devices_t device_type, uint16_t short_address, uint8_t endpoint)
{
    esp_rainmaker_gateway_joining_device_list_t *curr = end_device_list;
    while (curr) {
        if ((curr->zigbee_info.short_addr == short_address) && (curr->zigbee_info.endpoint == endpoint)) {
            ESP_LOGI(TAG, "existing end device back online");
            return ESP_OK;
        }
        curr = curr->next;
    }
    esp_app_rainmaker_add_end_device(device_type, short_address, endpoint, true);
    if (esp_rmaker_report_node_details() != ESP_OK) {
        ESP_LOGE(TAG, "Report node state failed.");
    } else {
        ESP_LOGI(TAG, "Add a device success with short_address:0x%x, endpoint:%d", short_address, endpoint);
    }
#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
    if (device_count > 0) {
        esp_event_post(M5STACK_DISPLAY_EVENT, DISPLAY_ZIGBEE_DEVICE_COUNT_EVENT, &device_count, sizeof(device_count), portMAX_DELAY);
    }
#endif
    return ESP_OK;
}

static void button_factory_reset_pressed_cb(void *arg, void *data)
{
    if (!perform_factory_reset) {
        ESP_LOGI(TAG, "Factory reset triggered. Release the button to start factory reset.");
        perform_factory_reset = true;
    }
}

static void button_factory_reset_released_cb(void *arg, void *data)
{
    if (perform_factory_reset) {
        ESP_LOGI(TAG, "Starting factory reset");
        esp_rmaker_factory_reset(RESET_DELAY, REBOOT_DELAY);
        perform_factory_reset = false;
}
}

static esp_err_t app_reset_button_register(button_handle_t btn_handle)
{
    if (!btn_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = ESP_OK;
    err |= iot_button_register_cb(btn_handle, BUTTON_LONG_PRESS_HOLD, button_factory_reset_pressed_cb, NULL);
    err |= iot_button_register_cb(btn_handle, BUTTON_PRESS_UP, button_factory_reset_released_cb, NULL);
    return err;
}

static void app_driver_init()
{
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = BUTTON_GPIO,
            .active_level = 0,
        },
    };
    app_reset_button_register(iot_button_create(&btn_cfg));
}

void esp_app_rainmaker_main()
{
    app_driver_init(); /* factory button register */

    /* Initialize NVS. */
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK( err );

    /* Initialize Wi-Fi. Note that, this should be called before esp_rmaker_node_init()
     */
    app_network_init();

    /* Initialize the ESP RainMaker Agent.
     * Note that this should be called after app_network_init() but before app_network_start()
     * */
    esp_rmaker_config_t rainmaker_cfg = {
        .enable_time_sync = false,
    };
    esp_rmaker_node_t *node = esp_rmaker_node_init(&rainmaker_cfg, "ESP RainMaker Gateway Device", "GW");
    if (!node) {
        ESP_LOGE(TAG, "Could not initialise node. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    /* Create a device and add the relevant parameters to it */
    zigbee_gw_device = esp_rmaker_zigbee_gateway_device_create("Zigbee_Gateway", NULL);
    esp_rmaker_device_add_cb(zigbee_gw_device, write_cb, NULL);
    esp_rmaker_node_add_device(node, zigbee_gw_device);

    for (size_t i = 0; i < ZIGBEE_MAX_DEVICE_COUNT; i++) {
        device_addrs[i] = 0xfffe;
    }

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

    /* Start the ESP RainMaker Agent */
    esp_rmaker_start();

#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
    app_register_m5stack_display_event_handler();
#endif

    /* Start the Wi-Fi.
     * If the node is provisioned, it will start connection attempts,
     * else, it will start Wi-Fi provisioning. The function will return
     * after a connection has been successfully established
     */
    err = app_network_start(POP_TYPE_RANDOM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not start Wifi. Aborting!!!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        abort();
    }

    resume_device();
}

/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include "sdkconfig.h"
#include <esp_app_rainmaker.h>
#include "esp_check.h"
#include "esp_netif.h"
#include "esp_vfs_eventfd.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_rcp_update.h"
#include "esp_coexist.h"
#include "esp_vfs_dev.h"
#include <esp_gateway_main.h>
#include <esp_gateway_control.h>

#if CONFIG_SW_COEXIST_ENABLE
#include "private/esp_coexist_internal.h"
#endif // CONFIG_SW_COEXIST_ENABLE
#if CONFIG_OPENTHREAD_SPINEL_ONLY
#include "esp_radio_spinel.h"
#endif // CONFIG_OPENTHREAD_SPINEL_ONLY
#if (!defined ZB_MACSPLIT_HOST && defined ZB_MACSPLIT_DEVICE)
#error Only Zigbee gateway host device should be defined
#endif

#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
#include "m5display.h"
#endif

static const char *TAG = "ESP_ZB_GATEWAY";

uint8_t gateway_ep = ESP_GW_ENDPOINT;
static uint16_t zigbee_cluster_list[] = { ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE };
/********************* Define functions **************************/

/* Note: Please select the correct console output port based on the development board in menuconfig */
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
esp_err_t esp_zb_gateway_console_init(void)
{
    esp_err_t ret = ESP_OK;
    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    /* Enable non-blocking mode on stdin and stdout */
    fcntl(fileno(stdout), F_SETFL, O_NONBLOCK);
    fcntl(fileno(stdin), F_SETFL, O_NONBLOCK);

    usb_serial_jtag_driver_config_t usb_serial_jtag_config = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ret = usb_serial_jtag_driver_install(&usb_serial_jtag_config);
    esp_vfs_usb_serial_jtag_use_driver();
    esp_vfs_dev_uart_register();
    return ret;
}
#endif

#if(CONFIG_AUTO_UPDATE_RCP)
static void zb_gateway_update_rcp(void)
{
    /* Deinit uart to transfer UART to the serial loader */
    esp_zb_rcp_deinit();
    if (esp_rcp_update() != ESP_OK) {
        esp_rcp_mark_image_verified(false);
    }
    esp_restart();
}

static void esp_zb_gateway_board_try_update(const char *rcp_version_str)
{
    char version_str[RCP_VERSION_MAX_SIZE];
    if (esp_rcp_load_version_in_storage(version_str, sizeof(version_str)) == ESP_OK) {
        ESP_LOGI(TAG, "Storage RCP Version: %s", version_str);
        if (strcmp(version_str, rcp_version_str)) {
            ESP_LOGI(TAG, "*** NOT MATCH VERSION! ***");
            zb_gateway_update_rcp();
        } else {
            ESP_LOGI(TAG, "*** MATCH VERSION! ***");
            esp_rcp_mark_image_verified(true);
        }
    } else {
        ESP_LOGI(TAG, "RCP firmware not found in storage, will reboot to try next image");
        esp_rcp_mark_image_verified(false);
        esp_restart();
    }
}

static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t rcp_fw_conf = {
        .base_path = "/rcp_fw", .partition_label = "rcp_fw", .max_files = 10, .format_if_mount_failed = false
    };
    esp_vfs_spiffs_register(&rcp_fw_conf);
    return ESP_OK;
}
#endif

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}

static esp_err_t zb_ias_zone_enroll_request_handler(const esp_zb_zcl_ias_zone_enroll_request_message_t *message)
{
    esp_zb_zcl_ias_zone_enroll_response_cmd_t resp_cmd = {
        .zcl_basic_cmd.dst_addr_u.addr_short = message->info.src_address.u.short_addr,
        .zcl_basic_cmd.dst_endpoint = message->info.src_endpoint,
        .zcl_basic_cmd.src_endpoint = ESP_GW_ENDPOINT,
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .enroll_rsp_code = ESP_ZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS,
        .zone_id = 1,
    };
    esp_zb_zcl_ias_zone_enroll_cmd_resp(&resp_cmd);
    return ESP_OK;
}

static esp_err_t zb_ias_zone_status_change_handler(const esp_zb_zcl_ias_zone_status_change_notification_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received status information: zone status: 0x%x, zone id: 0x%x\n", message->zone_status, message->zone_id);
    esp_app_rainmaker_update_device_param(message->info.src_address.u.short_addr, message->info.src_endpoint, (message->zone_status & 0x1));
    return ret;
}

static void esp_zigbee_write_ias_cie_address(uint16_t short_addr, uint8_t endpoint)
{
    esp_zb_ieee_addr_t ieee_addr;
    esp_zb_get_long_address(ieee_addr);
    esp_zb_zcl_attribute_t attr_field = {
        ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID,
        {ESP_ZB_ZCL_ATTR_TYPE_IEEE_ADDR, sizeof(esp_zb_ieee_addr_t), ieee_addr}};
    esp_zb_zcl_write_attr_cmd_t cmd_req = {
        .zcl_basic_cmd.dst_addr_u.addr_short = short_addr,
        .zcl_basic_cmd.dst_endpoint = endpoint,
        .zcl_basic_cmd.src_endpoint = ESP_GW_ENDPOINT,
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .clusterID = ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE,
        .attr_number = 1,
        .attr_field = &attr_field,
    };
    esp_zb_zcl_write_attr_cmd_req(&cmd_req);
}

static void simple_desc_cb(esp_zb_zdp_status_t zdo_status, esp_zb_af_simple_desc_1_1_t *simple_desc, void *user_ctx)
{
    device_params_t *device = (device_params_t *)user_ctx;
    uint16_t short_addr = device->short_addr;
    uint8_t endpoint = device->endpoint;
    free(device);
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "Simple desc response: status(%d), device_id(%d), app_version(%d), profile_id(0x%x), endpoint_ID(%d)", zdo_status,
                 simple_desc->app_device_id, simple_desc->app_device_version, simple_desc->app_profile_id, simple_desc->endpoint);
        switch (simple_desc->app_device_id) {
        case ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID:
            esp_app_rainmaker_add_joining_device(ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID, short_addr, endpoint);
            return;
        case ESP_ZB_HA_IAS_ZONE_ID:
            esp_zigbee_write_ias_cie_address(short_addr, endpoint);
            esp_app_rainmaker_add_joining_device(ESP_ZB_HA_IAS_ZONE_ID, short_addr, endpoint);
            return;
        default:
            break;
        }
        ESP_LOGW(TAG, "Unsupported device type!");
    }
}

void user_find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx)
{
    ESP_LOGI(TAG, "User find cb: response_status:%d, address:0x%x, endpoint:%d", zdo_status, addr, endpoint);
    if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS && addr != 0xFFFF) {
        esp_zb_zdo_simple_desc_req_param_t simple_desc_req;
        simple_desc_req.addr_of_interest = addr;
        simple_desc_req.endpoint = endpoint;
        device_params_t *device = (device_params_t *)malloc(sizeof(device_params_t));
        device->endpoint = endpoint;
        device->short_addr = addr;
        esp_zb_zdo_simple_desc_req(&simple_desc_req, simple_desc_cb, (void *)device);
    }
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_ENROLL_REQUEST_ID:
        ret = zb_ias_zone_enroll_request_handler((esp_zb_zcl_ias_zone_enroll_request_message_t *)message);
        break;
    case ESP_ZB_CORE_CMD_IAS_ZONE_ZONE_STATUS_CHANGE_NOT_ID:
        ret = zb_ias_zone_status_change_handler((esp_zb_zcl_ias_zone_status_change_notification_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee report(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    esp_zb_zdo_signal_device_annce_params_t *dev_annce_params = NULL;

    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_app_rainmaker_main(); // rainmaker task
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Start network formation");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_FORMATION);
        } else {
            ESP_LOGE(TAG, "Failed to initialize Zigbee stack (status: %d)", err_status);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_FORMATION:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t ieee_address;
            esp_zb_get_long_address(ieee_address);
            ESP_LOGI(TAG, "Formed network successfully (ieee_address: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d)",
                     ieee_address[7], ieee_address[6], ieee_address[5], ieee_address[4],
                     ieee_address[3], ieee_address[2], ieee_address[1], ieee_address[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel());
#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
            esp_event_post(M5STACK_DISPLAY_EVENT, DISPLAY_ZIGBEE_PANID_EVENT, NULL, 0, portMAX_DELAY);
#endif
        } else {
            ESP_LOGI(TAG, "Restart network formation (status: %d)", err_status);
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_FORMATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Network steering started");
        }
        break;
    case ESP_ZB_ZDO_SIGNAL_DEVICE_ANNCE:
        dev_annce_params = (esp_zb_zdo_signal_device_annce_params_t *)esp_zb_app_signal_get_params(p_sg_p);
        ESP_LOGI(TAG, "New device commissioned or rejoined (short: 0x%04hx)", dev_annce_params->device_short_addr);
        esp_zb_zdo_match_desc_req_param_t cmd_req;
        cmd_req.dst_nwk_addr = dev_annce_params->device_short_addr;
        cmd_req.addr_of_interest = dev_annce_params->device_short_addr;
        cmd_req.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
        cmd_req.num_in_clusters = sizeof(zigbee_cluster_list) / sizeof(zigbee_cluster_list[0]);
        cmd_req.num_out_clusters = 0;
        cmd_req.cluster_list = zigbee_cluster_list;
        esp_zb_zdo_match_cluster(&cmd_req, user_find_cb, NULL);
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %d, status: %d", sig_type, err_status);
        break;
    }
}

void rcp_error_handler(void)
{
#if(CONFIG_AUTO_UPDATE_RCP)
    ESP_LOGI(TAG, "Re-flashing RCP");
    zb_gateway_update_rcp();
#endif
    esp_restart();
}

#if CONFIG_OPENTHREAD_SPINEL_ONLY
static esp_err_t check_ot_rcp_version(void)
{
    char internal_rcp_version[RCP_VERSION_MAX_SIZE];
    ESP_RETURN_ON_ERROR(esp_radio_spinel_rcp_version_get(internal_rcp_version, ESP_RADIO_SPINEL_ZIGBEE), TAG, "Fail to get rcp version from radio spinel");
    ESP_LOGI(TAG, "Running RCP Version: %s", internal_rcp_version);
#if(CONFIG_AUTO_UPDATE_RCP)
    esp_zb_gateway_board_try_update(internal_rcp_version);
#endif
    return ESP_OK;
}
#endif

static void esp_zb_task(void *pvParameters)
{
  #if CONFIG_OPENTHREAD_SPINEL_ONLY
    esp_radio_spinel_register_rcp_failure_handler(rcp_error_handler, ESP_RADIO_SPINEL_ZIGBEE);
#endif  
    /* initialize Zigbee stack with Zigbee coordinator config */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZC_CONFIG();
    esp_zb_init(&zb_nwk_cfg);
#if CONFIG_OPENTHREAD_SPINEL_ONLY
    ESP_ERROR_CHECK(check_ot_rcp_version());
#endif
    /* set the attribute and cluster */
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(NULL);
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(NULL);
    esp_zb_attribute_list_t *esp_zb_time_cluster = esp_zb_time_cluster_create(NULL);
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_time_cluster(esp_zb_cluster_list, esp_zb_time_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    /* add created endpoint (cluster_list) to endpoint list */
    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = ESP_GW_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_HOME_GATEWAY_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_gateway_ep(esp_zb_ep_list, esp_zb_cluster_list, endpoint_config);
    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    /* initiate Zigbee Stack start without zb_send_no_autostart_signal auto-start */
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
    esp_rcp_update_deinit();
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };

    ESP_ERROR_CHECK(esp_zb_platform_config(&config));
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    ESP_ERROR_CHECK(esp_zb_gateway_console_init());
#endif

#if CONFIG_M5STACK_ZIGBEE_GATEWAY_BOARD
    app_m5stack_display_start();
#endif

#if(CONFIG_AUTO_UPDATE_RCP)
    esp_rcp_update_config_t rcp_update_config = ESP_ZB_RCP_UPDATE_CONFIG();
    ESP_ERROR_CHECK(init_spiffs());
    ESP_ERROR_CHECK(esp_rcp_update_init(&rcp_update_config));
#endif
    xTaskCreate(esp_zb_task, "Zigbee_main", 8192, NULL, 5, NULL);
}

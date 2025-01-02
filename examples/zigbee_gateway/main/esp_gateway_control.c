/* Zigbee Gateway Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <esp_err.h>
#include <esp_zigbee_core.h>
#include <esp_gateway_control.h>

static const char *TAG = "esp_gateway_control";
extern uint8_t gateway_ep;

esp_err_t esp_gateway_control_permit_join(uint8_t permit_duration)
{
    esp_zb_zdo_permit_joining_req_param_t cmd_req;
    cmd_req.dst_nwk_addr = 0x0000;
    cmd_req.permit_duration = permit_duration;
    cmd_req.tc_significance = 1;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zdo_permit_joining_req(&cmd_req, NULL, NULL);
    esp_zb_lock_release();
    return ESP_OK;
}

esp_err_t esp_gateway_control_secur_ic_add(esp_zigbee_ic_mac_address_t *IC_MacAddress, esp_zb_secur_ic_type_t ic_type)
{
    return esp_zb_secur_ic_add(IC_MacAddress->addr, ic_type, IC_MacAddress->ic);
}

esp_err_t esp_gateway_control_light_on_off(bool on_off, device_params_t *device)
{
    esp_zb_zcl_on_off_cmd_t cmd_req;
    uint8_t cmd_id = (on_off) ? (ESP_ZB_ZCL_CMD_ON_OFF_ON_ID) : (ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID);
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = device->short_addr;
    cmd_req.zcl_basic_cmd.dst_endpoint = device->endpoint;
    cmd_req.zcl_basic_cmd.src_endpoint = gateway_ep;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = cmd_id;
    ESP_LOGI(TAG, "send 'on_off' command: %s", on_off ? "true" : "false");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
    return ESP_OK;
}

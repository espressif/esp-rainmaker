/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "matter_commissioning_window_management.h"
#include "matter_commissioning_window_management_std.h"
#include "platform/PlatformManager.h"

#include <cstring>
#include <esp_check.h>
#include <esp_rmaker_core.h>

#include <app/server/Server.h>
#include <setup_payload/SetupPayload.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/ManualSetupPayloadGenerator.h>
#include <setup_payload/OnboardingCodesUtil.h>

constexpr char *TAG = "MatterCWM";
static esp_rmaker_device_t *s_matter_cwm_service;

esp_err_t matter_commissioning_window_parameters_update()
{
#ifdef CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER
    char manualPairingCodeBuffer[chip::kManualSetupLongCodeCharLength + 1];
    chip::MutableCharSpan manualPairingCode(manualPairingCodeBuffer);
    char qrCodeBuffer[chip::QRCodeBasicSetupPayloadGenerator::kMaxQRCodeBase38RepresentationLength + 1];
    chip::MutableCharSpan qrCode(qrCodeBuffer);
    chip::RendezvousInformationFlags flags(chip::RendezvousInformationFlag::kOnNetwork);
    GetQRCode(qrCode, flags);
    GetManualPairingCode(manualPairingCode, flags);
    esp_rmaker_param_val_t val;
    val.type = RMAKER_VAL_TYPE_STRING;
    val.val.s = qrCode.data();
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_type(s_matter_cwm_service, ESP_RMAKER_PARAM_MATTER_QRCODE);
    esp_rmaker_param_val_t *param_val = esp_rmaker_param_get_val(param);
    if (param_val && strncmp(param_val->val.s, val.val.s, strlen(val.val.s))) { // If the qrcode is the same, no need update and report to cloud
        ESP_RETURN_ON_ERROR(esp_rmaker_param_update(param, val), TAG, "Failed to update QR code");
        val.val.s = manualPairingCode.data();
        param = esp_rmaker_device_get_param_by_type(s_matter_cwm_service, ESP_RMAKER_PARAM_MATTER_MANUALCODE);
        ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update ManualCode");
    }
#endif
    return ESP_OK;
}

esp_err_t matter_commissioning_window_status_update(bool open)
{
    esp_rmaker_param_val_t val;
    val.type = RMAKER_VAL_TYPE_BOOLEAN;
    val.val.b = open;
    esp_rmaker_param_t *param = esp_rmaker_device_get_param_by_type(s_matter_cwm_service,
                                                                    ESP_RMAKER_PARAM_MATTER_COMMISSIONING_WINDOW_OPEN);
    esp_rmaker_param_val_t *param_val = esp_rmaker_param_get_val(param);
    if (param_val && (param_val->val.b != open)) {
        ESP_RETURN_ON_ERROR(esp_rmaker_param_update_and_report(param, val), TAG, "Failed to update WindowOpen status");
    }
    return ESP_OK;
}

static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    if (!s_matter_cwm_service) {
        return ESP_ERR_INVALID_STATE;
    }
    if (strcmp(esp_rmaker_param_get_type(param), ESP_RMAKER_PARAM_MATTER_COMMISSIONING_WINDOW_OPEN) == 0 &&
        ctx->src != ESP_RMAKER_REQ_SRC_INIT) {
        if (val.type != RMAKER_VAL_TYPE_BOOLEAN) {
            return ESP_ERR_INVALID_ARG;
        }
        if (val.val.b) {
            chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t arg){
                chip::Server::GetInstance().GetCommissioningWindowManager().OpenBasicCommissioningWindow(); });
        } else {
            chip::DeviceLayer::PlatformMgr().ScheduleWork([](intptr_t arg){
                chip::Server::GetInstance().GetCommissioningWindowManager().CloseCommissioningWindow(); });
        }
        esp_rmaker_param_update(param, val);
    }
    return ESP_OK;
}

esp_err_t matter_commissioning_window_management_enable()
{
    s_matter_cwm_service = matter_commissioning_window_management_service_create("MatterCWM", write_cb, nullptr, nullptr);
    if (!s_matter_cwm_service) {
        ESP_LOGE(TAG, "Failed to create Matter Commissioning Window Management service");
        return ESP_FAIL;
    }
    return esp_rmaker_node_add_device(esp_rmaker_get_node(), s_matter_cwm_service);
}

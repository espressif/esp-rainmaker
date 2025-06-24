/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_check.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_rmaker_utils.h>
#include <esp_matter_controller_client.h>
#include <esp_matter_controller_utils.h>
#include <esp_matter_core.h>
#include <controller_rest_apis.h>

#include <credentials/CHIPCert.h>
#include <credentials/GroupDataProvider.h>
#include <crypto/CHIPCryptoPAL.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>
#include <lib/core/Optional.h>
#include <lib/support/Span.h>
#include <lib/support/ScopedBuffer.h>
#include <stdint.h>

#include <app_matter_controller_callback.h>
#include <app_matter_controller.h>
#include <app_matter_controller_creds_issuer.h>
#include <app_matter_device_manager.h>


#define TAG "MatterController"

static esp_err_t app_matter_controller_authorize(matter_controller_handle_t *handle)
{
    esp_err_t err = ESP_OK;
    if (!handle->base_url || !handle->user_token || handle->access_token) {
        return ESP_ERR_INVALID_ARG;
    }
    handle->access_token = (char *)MEM_CALLOC_EXTRAM(1800, 1);
    if (!handle->access_token) {
        return ESP_ERR_NO_MEM;
    }
    if ((err = fetch_access_token(handle->base_url, handle->user_token, handle->access_token, 1800)) != ESP_OK) {
        free(handle->access_token);
        ESP_LOGE(TAG, "Failed on fetch access token");
    }
    return err;
}

static esp_err_t app_matter_controller_fetch_matter_fabric_id(matter_controller_handle_t *handle)
{
    if (!handle->base_url || !handle->access_token || !handle->rmaker_group_id || handle->matter_fabric_id != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return fetch_matter_fabric_id(handle->base_url, handle->access_token, handle->rmaker_group_id,
                                  &handle->matter_fabric_id);
}

static esp_err_t app_matter_controller_fetch_fabric_ipk(matter_controller_handle_t *handle, chip::MutableByteSpan &ipk)
{
    if (!handle->base_url || !handle->access_token || !handle->rmaker_group_id) {
        return ESP_ERR_INVALID_ARG;
    }
    return fetch_fabric_ipk(handle->base_url, handle->access_token, handle->rmaker_group_id, ipk.data(), ipk.size());
}

static esp_err_t app_matter_controller_setup_controller(matter_controller_handle_t *handle)
{
    static bool controller_setup = false;
    if (controller_setup) {
        return ESP_OK;
    }
    if (handle->matter_node_id > 0) {
        return ESP_ERR_INVALID_STATE;
    }
    uint8_t ipk_value[chip::Crypto::CHIP_CRYPTO_SYMMETRIC_KEY_LENGTH_BYTES];
    chip::MutableByteSpan ipk_span(ipk_value);
    if (!handle->matter_noc_installed) {
        ESP_RETURN_ON_ERROR(app_matter_controller_fetch_fabric_ipk(handle, ipk_span), TAG, "Failed on fetching IPK");
    } else {
        ESP_LOGI(TAG, "NOC chain has been installed, setup controller with an empty IPK");
        ipk_span.reduce_size(0);
    }
    esp_matter::lock::chip_stack_lock(portMAX_DELAY);
    esp_err_t err = esp_matter::controller::matter_controller_client::get_instance().setup_controller(ipk_span);
    esp_matter::lock::chip_stack_unlock();
    controller_setup = true;
    return err;
}

static esp_err_t app_matter_controller_update_noc(matter_controller_handle_t *handle)
{
    // TODO: There are some issues for connectedhomeip API to update controller's NOC
    return ESP_OK;
}

static esp_err_t app_matter_controller_update_device_list(matter_controller_handle_t *handle)
{
    return update_device_list(handle);
}

esp_err_t app_matter_controller_callback(matter_controller_handle_t *handle, matter_controller_callback_type_t type)
{
    switch (type) {
    case MATTER_CONTROLLER_CALLBACK_TYPE_AUTHORIZE: {
        return app_matter_controller_authorize(handle);
        break;
    }
    case MATTER_CONTROLLER_CALLBACK_TYPE_QUERY_MATTER_FABRIC_ID:{
        return app_matter_controller_fetch_matter_fabric_id(handle);
        break;
    }
    case MATTER_CONTROLLER_CALLBACK_TYPE_SETUP_CONTROLLER: {
        return app_matter_controller_setup_controller(handle);
        break;
    }
    case MATTER_CONTROLLER_CALLBACK_TYPE_UPDATE_CONTROLLER_NOC: {
        return app_matter_controller_update_noc(handle);
        break;
    }
    case MATTER_CONTROLLER_CALLBACK_TYPE_UPDATE_DEVICE: {
        return app_matter_controller_update_device_list(handle);
        break;
    }
    default:
        break;
    }
    return ESP_OK;
}

// Copyright 2024 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <json_parser.h>
#include <json_generator.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_schedule.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_ota.h>
#include <esp_ota_ops.h>
#include <esp_rmaker_factory.h>
#include <esp_task.h>

#include <freertos/task.h>
#include <freertos/event_groups.h>

#include "esp_rmaker_client_data.h"
#include "esp_rmaker_ota_https.h"
#include "esp_rmaker_ota_https_internal.h"
#include "rmaker_node_api_constants.h"

#ifdef CONFIG_OTA_HTTPS_AUTOFETCH_ENABLED
    #include <esp_timer.h>
    /* Convert hours to microseconds */
    #define OTA_AUTOFETCH_PERIOD CONFIG_OTA_HTTPS_AUTOFETCH_PERIOD * 3600 * 1000000
#endif

static const char *TAG = "esp_rmaker_ota_https";
static esp_rmaker_ota_https_t *g_ota_https_data; 
static EventGroupHandle_t event_group_apply_update;
#define OTA_APPLIED_BIT BIT0

static const esp_rmaker_ota_config_t ota_default_config = {
    .ota_cb = esp_rmaker_ota_default_cb
};

static esp_err_t ota_https_nvs_erase_job_id(void){
    nvs_handle nvs;
    if(nvs_open_from_partition(OTA_HTTPS_NVS_PART_NAME, OTA_HTTPS_NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK){
        if(nvs_erase_key(nvs, OTA_HTTPS_JOB_ID_NVS_NAME) != ESP_OK){
            return ESP_OK;
        }
    }

    return ESP_FAIL;
}

esp_err_t esp_rmaker_ota_https_report(char *ota_job_id, ota_status_t status, char *additional_info)
{

    /* Actual url will be 59 bytes. */
    char ota_report_url[64];
    snprintf(ota_report_url, 64, "%s/%s", NODE_API_ENDPOINT_BASE, NODE_API_ENDPOINT_SUFFIX_REPORT);

    char publish_payload[200];
    json_gen_str_t jstr;
    json_gen_str_start(&jstr, publish_payload, sizeof(publish_payload), NULL, NULL);
    json_gen_start_object(&jstr);

    if(ota_job_id){
        json_gen_obj_set_string(&jstr, "ota_job_id", ota_job_id);
    } else {
        /* Reporting after applying and reboot, we need to fetch ota_job_id from NVS */
        nvs_handle nvs;
        if(nvs_open_from_partition(OTA_HTTPS_NVS_PART_NAME, OTA_HTTPS_NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK){
            char job_id[64] = {0};
            size_t job_id_len = sizeof(job_id);
            if(nvs_get_blob(
                nvs, OTA_HTTPS_JOB_ID_NVS_NAME, &job_id, &job_id_len) == ESP_OK){
                json_gen_obj_set_string(&jstr, "ota_job_id", job_id);
            } else {
                ESP_LOGE(TAG, "Failed to report: ota_job_id not found in NVS");
                return ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "Failed to open NVS for reading ota_job_id");
            return ESP_FAIL;
        }
    }

    json_gen_obj_set_string(&jstr, "status", esp_rmaker_ota_status_to_string(status));
    json_gen_obj_set_string(&jstr, "additional_info", additional_info);
    json_gen_end_object(&jstr);
    json_gen_str_end(&jstr);

    const char *client_cert = esp_rmaker_get_client_cert();
    const char *client_key = esp_rmaker_get_client_key();

    if (!client_cert || !client_key) {
        if (client_cert) {
            free((char *)client_cert);
        }
        if (client_key) {
            free((char *)client_key);
        }
        return ESP_ERR_INVALID_STATE;
    }

    esp_http_client_config_t http_config = {
        .url = ota_report_url,
        .client_cert_pem = client_cert,
        .client_key_pem = client_key,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_POST
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    if(!client){
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        free((char *)client_cert);
        free((char *)client_key);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_http_client_set_post_field(client, publish_payload, strlen(publish_payload));
    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to perform HTTP request.");
        err = ESP_FAIL;
        goto end;
    }

    int response_status_code = esp_http_client_get_status_code(client);
    if (response_status_code != 200)
    {
        ESP_LOGE(TAG, "Failed to report status: %d", response_status_code);
        ESP_LOGE(TAG, "writing to server: %s, payload: %s", ota_report_url, publish_payload);
        err = ESP_FAIL;
    }

end:
    esp_http_client_cleanup(client);
    free((char *)client_cert);
    free((char *)client_key);
    return err;
}

static void esp_rmaker_ota_https_finish(esp_rmaker_ota_https_t *ota){
    if (!ota) {
        return;
    }

    if (ota->ota_url){
        free(ota->ota_url);
        ota->ota_url = NULL;
    }

    if (ota->fw_version){
        free(ota->fw_version);
        ota->fw_version = NULL;
    }

    if (ota->ota_job_id){
        free(ota->ota_job_id);
        ota->ota_job_id = NULL;
    }
    
    if(ota->metadata){
        free(ota->metadata);
        ota->metadata = NULL;
    }

    if (ota->filesize){
        ota->filesize = 0;
    }

    ota->ota_in_progress = false;
}

    
static void _apply_ota_task(void *arg){
    esp_rmaker_ota_https_t *ota = (esp_rmaker_ota_https_t *)arg;
    
    if(!ota->ota_cb){
        ESP_LOGE(TAG, "OTA Callback not registered");
        goto end;
    }

    esp_rmaker_ota_data_t ota_data;
    ota_data.fw_version = ota->fw_version;
    ota_data.filesize = ota->filesize;
    ota_data.metadata = ota->metadata;
    ota_data.ota_job_id = ota->ota_job_id;
    ota_data.priv = ota->priv;
    ota_data.report_fn = esp_rmaker_ota_https_report;
    ota_data.url = ota->ota_url;

    if (ota->ota_cb(NULL, &ota_data) != ESP_OK){
        ESP_LOGE(TAG, "Failed to apply OTA");
    }

end:
    xEventGroupSetBits(event_group_apply_update, OTA_APPLIED_BIT);
    vTaskDelete(NULL);
}

static esp_err_t apply_ota(esp_rmaker_ota_https_t *ota)
{
    event_group_apply_update = xEventGroupCreate();
    if (!event_group_apply_update){
        ESP_LOGE(TAG, "Unable to create event group for applying OTA");
        return ESP_ERR_NO_MEM;
    }

    if(xTaskCreate(_apply_ota_task, "apply_ota", CONFIG_OTA_HTTPS_APPLY_STACK_SIZE, ota, 5, NULL) != pdPASS){
        ESP_LOGE(TAG, "Failed to create task for applying OTA");
        vEventGroupDelete(event_group_apply_update);
        return ESP_ERR_NO_MEM;
    }

    xEventGroupWaitBits(event_group_apply_update, OTA_APPLIED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    vEventGroupDelete(event_group_apply_update);
    return ESP_OK;
}

static esp_err_t handle_fetched_data(char* json_payload)
{
    esp_err_t err = ESP_OK;
    if(!json_payload){
        return ESP_ERR_INVALID_ARG;
    }

    jparse_ctx_t jctx;    

    int ret = json_parse_start(&jctx, json_payload, strlen(json_payload));
    if (ret != OS_SUCCESS){
        ESP_LOGE(TAG, "Failed to start json parser.");
        return ESP_FAIL;
    };

    /* RainMaker node API returns 404 with a specific error code when there is no OTA available.
     * We first check if `error_code` is present in the response. 
     * If we find it, we can further check if that denotes no pending OTA or some other error
     * Otherwise, the reponse contains actual OTA info and we can parse it accordingly */
    
    int error_code = -1;
    ret = json_obj_get_int(&jctx, NODE_API_FIELD_ERROR_CODE, &error_code);
    if(error_code == -1){
        /* OTA update found */
        char* buff;
        int len;
        esp_rmaker_ota_https_t *ota = g_ota_https_data;
        ota->ota_in_progress = true;

        /* Extract JOB_ID from payload */
        ret = json_obj_get_strlen(&jctx, NODE_API_FIELD_JOB_ID, &len);
        if (ret) {
            ESP_LOGE(TAG, "ota_job_id not found in OTA update.");
            err = ESP_ERR_INVALID_ARG;
            goto end;
        }
        
        buff = MEM_ALLOC_EXTRAM(len+1);
        if(!buff){
            ESP_LOGE(TAG, "Could not allocate %d bytes for OTA JOBID", len);
            err = ESP_ERR_NO_MEM;
            goto end;
        }
        json_obj_get_string(&jctx, NODE_API_FIELD_JOB_ID, buff, len+1);
        ota->ota_job_id = buff;
        buff = NULL;

        nvs_handle nvs;
        if(nvs_open_from_partition(OTA_HTTPS_NVS_PART_NAME, OTA_HTTPS_NVS_NAMESPACE, NVS_READWRITE, &nvs) == ESP_OK){
            nvs_set_blob(nvs, OTA_HTTPS_JOB_ID_NVS_NAME, ota->ota_job_id, len);
            nvs_close(nvs);
        }

        /* Extract URL from payload */
        ret = json_obj_get_strlen(&jctx, NODE_API_FIELD_URL, &len);
        if (ret) {
            ESP_LOGE(TAG, "Url not found in OTA update.");
            esp_rmaker_ota_https_report(ota->ota_job_id, OTA_STATUS_REJECTED, "URL not present");
            err = ESP_ERR_INVALID_ARG;
            goto end;
        }

        buff = MEM_ALLOC_EXTRAM(len+1);
        if(!buff){
            ESP_LOGE(TAG, "Could not allocate %d bytes for OTA URL", len);
            esp_rmaker_ota_https_report(ota->ota_job_id, OTA_STATUS_REJECTED, "Failed to allocate memory for URL");
            err = ESP_ERR_NO_MEM;
            goto end;
        }
        json_obj_get_string(&jctx, NODE_API_FIELD_URL, buff, len+1);
        ota->ota_url = buff;
        buff = NULL;

        /* Try to extract metadata from payload 
         * Metadata is sent in the response only if it is applicable for a job.
         * Hence OTA will not be rejected if metadata key is not found in JSON.
         */
        
        /* Responding to time information requires rainmaker time server and a otafetch callback.
         * Disabling metadata parsing until it is implemented.
         */

        #ifdef ESP_RMAKER_OTA_HTTPS_EXTRACT_METADATA
            ret = json_obj_get_object_strlen(&jctx, NODE_API_FIELD_METADATA, &len);
            if (!ret) {
                ESP_LOGI(TAG, "Metadata present for OTA Job ");
                buff = MEM_ALLOC_EXTRAM(len+1);
                if(!buff){
                    ESP_LOGE(TAG, "Could not allocate %d bytes for OTA metadata", len);
                    esp_rmaker_ota_https_report(ota->ota_job_id, OTA_STATUS_REJECTED, "Failed to allocate memory for metadata");
                    err = ESP_ERR_NO_MEM;
                    goto end;
                }

                json_obj_get_object_str(&jctx, NODE_API_FIELD_METADATA, buff, len+1);
                ota->metadata = buff;
                buff = NULL;
            } 
            /* Extract Firmware Version from payload */
            ret = json_obj_get_strlen(&jctx, NODE_API_FIELD_FW_VERSION, &len);
            buff = MEM_ALLOC_EXTRAM(len+1);
            if(buff){
                json_obj_get_string(&jctx, NODE_API_FIELD_FW_VERSION, buff, len+1);
                ota->fw_version = buff;
                buff = NULL;
            }
        #endif
        

        /* Extract File Size from payload */
        int filesize;
        json_obj_get_int(&jctx, NODE_API_FIELD_FILE_SIZE, &filesize);
        ota->filesize = filesize;

        ESP_LOGD(TAG, "Parsed OTA Info-\nurl=%s\nota_job_id=%s\nfw_version=%s\nfilesize=%d", ota->ota_url, ota->ota_job_id, ota->fw_version, ota->filesize);
        if (!ota->ota_cb){
            ESP_LOGE(TAG, "Failed to process OTA: callback not found");
            err = ESP_ERR_INVALID_ARG;
            goto end;
        }

        /* The default callback from esp_rmaker_ota.c does not close esp_https_ota client
         * before reporting rejected(due to for e.g., invalid project version/name).
         * Hence, creating a separate task for this purpose.
         */
        apply_ota(ota);

    } else {
        /* Error */
        if(error_code == NODE_API_ERROR_CODE_NO_UPDATE_AVAILABLE){
            ESP_LOGI(TAG, "No OTA update available");
            goto end;
        }
        
        char* description_buff;
        int description_len;

        json_obj_get_strlen(&jctx, NODE_API_FIELD_DESCRIPTION, &description_len);
        description_buff = malloc(description_len+1); /* Also accounting NULL byte */

        if(description_buff){
            json_obj_get_string(&jctx, NODE_API_FIELD_DESCRIPTION, description_buff, description_len+1);
            description_buff[description_len] = '\0';
            ESP_LOGE(TAG, "Error while fetching OTA update - %s", description_buff);
            free(description_buff);
        }
    }

end:
    json_parse_end(&jctx);
    esp_rmaker_ota_https_finish(g_ota_https_data);
    return err;
}

esp_err_t esp_rmaker_ota_https_fetch(void)
{
    esp_rmaker_ota_https_t *ota = g_ota_https_data;

    if (!ota){
        return ESP_ERR_INVALID_STATE;
    }

    if (ota->ota_in_progress){
        ESP_LOGW(TAG, "Skipping OTA Fetch as an OTA is already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    /* Buffer size according to default API endpoint.
     * May need more space for other URLS.
     */
    char ota_fetch_url[64];
    snprintf(ota_fetch_url, 64, "%s/%s", NODE_API_ENDPOINT_BASE, NODE_API_ENDPOINT_SUFFIX_FETCH);

    const char *client_cert = esp_rmaker_get_client_cert();
    const char *client_key = esp_rmaker_get_client_key();

    if (!client_cert || !client_key) {
        if(client_cert){
            free((char *)client_cert);
        }
        if (client_key) {
            free((char *)client_key);
        }
        return ESP_ERR_INVALID_STATE;
    }

    esp_http_client_config_t http_config = {
        .url = ota_fetch_url,
        .client_cert_pem = client_cert,
        .client_key_pem = client_key,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .method = HTTP_METHOD_GET
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    esp_err_t err = ESP_OK;
    if(!client){
        ESP_LOGE(TAG, "Failed to initialize HTTP client.");
        err = ESP_FAIL;
        goto ret;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");

    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP Connection.");
        goto end;
    }

    int content_len = esp_http_client_fetch_headers(client);

    /* Reserving an extra byte for NULL character. */
    char *buff = MEM_ALLOC_EXTRAM(content_len+1);
    if (!buff) {
        ESP_LOGE(TAG, "Failed to allocate memory for response buffer.");
        err = ESP_ERR_NO_MEM;
        goto end;
    }

    int read_len = esp_http_client_read(client, buff, content_len);
    if (read_len < 0) {
        ESP_LOGE(TAG, "Failed to read HTTP Response.");
        err = ESP_FAIL;
    } else {
        buff[read_len] = '\0';
        ESP_LOGI(TAG, "Received OTA: %s", buff);
        if (esp_http_client_cleanup(client) != ESP_OK ){
            ESP_LOGW(TAG, "Failed to cleanup HTTP client before handling otafetch response.");
        }
        err = handle_fetched_data(buff);
        if (err != ESP_OK){
            ESP_LOGE(TAG, "Failed to handle received OTA information: %s", esp_err_to_name(err));
            err = ESP_FAIL;
        }
        goto ret; /* HTTP Client is already cleared */
    }

    free(buff);
end:
    esp_http_client_cleanup(client);
ret:
    free((char *)client_cert);
    free((char *)client_key);
    return err;
}



#ifdef CONFIG_OTA_HTTPS_AUTOFETCH_ENABLED
void esp_rmaker_ota_https_autofetch_cb(void *priv_data)
{
    if(esp_rmaker_ota_https_fetch() != ESP_OK){
        ESP_LOGE(TAG, "Failed to fetch OTA update");
    };
}
static void esp_rmaker_ota_https_register_timer(esp_rmaker_ota_https_t *ota)
{
    if (!ota){
        return;
    }

    esp_timer_create_args_t timer_config = {
        .name = "ota_https_autofetch",
        .callback = esp_rmaker_ota_https_autofetch_cb,
        .arg = (void *)ota,
        .dispatch_method = ESP_TIMER_TASK,
    };
    
    if(esp_timer_create(&timer_config, &ota->autofetch_timer) == ESP_OK){
        /* Timer period conversion for silencing overflow warning.
         * Theoretically overflow occures if 5124095574 hours provided.
         */
        esp_timer_start_periodic(ota->autofetch_timer, (uint64_t) OTA_AUTOFETCH_PERIOD);
    } else {
        ESP_LOGE(TAG, "Failed to enable autofetch.");
    }
}
#endif

esp_err_t esp_rmaker_ota_https_mark_valid(void)
{
    esp_rmaker_ota_https_t *ota = g_ota_https_data;
    if (!ota){
        return ESP_ERR_INVALID_STATE;
    }

    ota->ota_valid = true;

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state != ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "OTA Already marked as valid");
            return ESP_ERR_INVALID_STATE;
        }
    }

    if (!ota){
        return ESP_ERR_INVALID_STATE;
    }
    esp_ota_mark_app_valid_cancel_rollback();
    if(esp_timer_stop(ota->rollback_timer) != ESP_OK){
        ESP_LOGW(TAG, "Failed to stop rollback timemr.");
        /* Not returning failure since OTA is already marked as valid and rollback will eventually fail. */
    }

    ota->ota_in_progress = false;
    return ESP_OK;
}

esp_err_t esp_rmaker_ota_https_mark_invalid(void)
{
    return esp_rmaker_ota_mark_invalid();
}

static void esp_rmaker_ota_https_rollback(void *args) 
{
    if(g_ota_https_data -> ota_valid){
        return;
    }
    ESP_LOGE(TAG, "Unable to verify firmware. Rolling back.");
    esp_rmaker_ota_https_mark_invalid();
}

static esp_err_t ota_check_wifi(esp_rmaker_ota_https_t *ota)
{
    esp_timer_create_args_t timer_args = {
        .name = "OTA HTTPS Rollback Timer",
        .callback = esp_rmaker_ota_https_rollback,
        .arg = (void *) ota,
        .dispatch_method = ESP_TIMER_TASK,
    };
    esp_err_t err =  esp_timer_create(&timer_args, &ota->rollback_timer);
    if (err != ESP_OK){
        ESP_LOGE(TAG, "Failed to create rollback timer");
        return ESP_FAIL;
    }

    esp_timer_start_once(ota->rollback_timer, (CONFIG_OTA_HTTPS_ROLLBACK_PERIOD * 1000 * 1000));

    /* Verify OTA by trying to send OTA successful report */

    while(true) {
        if (esp_rmaker_ota_https_report(NULL, OTA_STATUS_SUCCESS, "OTA Upgrade finished and verified successfully") == ESP_OK){
            ESP_LOGI(TAG, "Sucessfully reported success for OTA image.");
            if(esp_rmaker_ota_https_mark_valid() != ESP_OK){
                ESP_LOGE(TAG, "Failed to mark OTA as valid after reporting success.");
                return ESP_FAIL;
            }

            if (ota_https_nvs_erase_job_id() != ESP_OK) {
                ESP_LOGW(TAG, "Failed to erase OTA ID from NVS.");
            }
            ESP_LOGI(TAG, "OTA Firmware verification successful");
            return ESP_OK;
        }

        /* Wait 5 seconds before retry */
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }

    return ESP_OK;
}

static void esp_rmaker_ota_https_manage_rollback(esp_rmaker_ota_https_t *ota)
{
    if (!ota){
        ESP_LOGE(TAG, "manage_rollback failed due to invalid argument");
        return;
    }

    bool validation_pending = esp_rmaker_ota_is_ota_validation_pending();
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        ESP_LOGI(TAG, "OTA state = %d", ota_state);
        /* Not checking for CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE here because the firmware may have
         * it disabled, but bootloader may have it enabled, in which case, we will have to
         * handle this state.
         */
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "First Boot after an OTA");
            /* Run diagnostic function */
            esp_rmaker_ota_diag_status_t diag_status = OTA_DIAG_STATUS_SUCCESS;
            if (ota->ota_diag) {
                esp_rmaker_ota_diag_priv_t ota_diag_priv = {
                    .state = OTA_DIAG_STATE_INIT,
                    .rmaker_ota = validation_pending
                };
                diag_status = ota->ota_diag(&ota_diag_priv, ota->priv);
            }
            if (diag_status != OTA_DIAG_STATUS_FAIL) {
                ESP_LOGI(TAG, "Diagnostics completed successfully! Continuing execution ...");
                ota->ota_in_progress = true;
                ota_check_wifi(ota);
                ota->ota_in_progress = false;
            } else {
                ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
                esp_rmaker_ota_https_mark_invalid();
            }
        } else {
            if (validation_pending) {
                esp_rmaker_ota_erase_rollback_flag();
                if(esp_rmaker_ota_https_report(NULL, OTA_STATUS_REJECTED, "Firmware rolled back") != ESP_OK){
                    ESP_LOGW(TAG, "Failed to report firmware rolled back.");
                }
                ota_https_nvs_erase_job_id();
            }

        }
    } 
}

esp_err_t esp_rmaker_ota_https_enable(esp_rmaker_ota_config_t *ota_config)
{
    ESP_LOGI(TAG, "Enabling OTA over HTTPS");
    /* Check if OTA is already enabled*/
    static bool ota_https_enabled;
    if (ota_https_enabled){
        ESP_LOGI(TAG, "HTTPS OTA already initialised");
        return ESP_ERR_INVALID_SIZE;
    }

    esp_rmaker_ota_https_t *ota = MEM_CALLOC_EXTRAM(1, sizeof(esp_rmaker_ota_https_t));
    if (!ota){
        ESP_LOGE(TAG, "Unable to allocate memory for storing OTA information");
        return ESP_ERR_NO_MEM;
    }

    if (!ota_config){
        ota_config = (esp_rmaker_ota_config_t *) &ota_default_config;
    }

    if (ota_config->ota_cb){
        ota->ota_cb = ota_config->ota_cb;
        ota->priv = ota_config->priv;
    } else {
        ota->ota_cb = &esp_rmaker_ota_default_cb;
        ota->priv = NULL;
    }

#ifdef CONFIG_OTA_HTTPS_AUTOFETCH_ENABLED
    esp_rmaker_ota_https_register_timer(ota);
#endif
    g_ota_https_data = ota;
    esp_rmaker_ota_https_manage_rollback(ota);
    return ESP_OK;
}

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

#pragma once

#include <esp_rmaker_ota.h>
#include <esp_timer.h>

typedef struct {
    esp_rmaker_ota_cb_t ota_cb;
    esp_rmaker_post_ota_diag_t ota_diag;
    void *priv;
    bool ota_in_progress;
    char *ota_url;
    char *fw_version;
    char *ota_job_id;
    char *metadata;
    bool ota_valid;
    int filesize;
    esp_timer_handle_t autofetch_timer;
    esp_timer_handle_t rollback_timer;
} esp_rmaker_ota_https_t;

/* TODO: find a way to avoid this value duplication */
#define OTA_HTTPS_NVS_PART_NAME     "nvs"
#define OTA_HTTPS_NVS_NAMESPACE    "rmaker_ota"
#define OTA_HTTPS_JOB_ID_NVS_NAME  "rmaker_ota_id"
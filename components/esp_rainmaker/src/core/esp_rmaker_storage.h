// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
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
#include <esp_err.h>

/** Intialise ESP RainMaker Storage
 *
 * This API is internally called by esp_rmaker_init(). Applications may call this
 * only if access to the ESP RainMaker storage is required before esp_rmaker_init().
 *
 * @return ESP_OK n success
 * @return error on failure
 */
esp_err_t esp_rmaker_storage_init();

/** Get data from ESP RainMaker storage
 *
 * This API will return NULL terminated keys/certificates
 * from the ESP RainMaker storage.
 *
 * @note This API allocates memory in the heap to hold the value. Once finished,
 * please free this using free().
 *
 * @param[in] key A NULL terminated key indicating the entity to be fetched
 *
 * @return Pointer to a NULL terminated string on success
 * @return NULL on error
 */
char *esp_rmaker_storage_get(const char *key);
esp_err_t esp_rmaker_storage_set(const char *key, void *data, size_t len);

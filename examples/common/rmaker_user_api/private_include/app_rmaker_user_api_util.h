/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string.h>
#include <esp_heap_caps.h>
#include <sdkconfig.h>

#ifdef __cplusplus
extern "C" {
#endif

#if (CONFIG_SPIRAM_SUPPORT && (CONFIG_SPIRAM_USE_CAPS_ALLOC || CONFIG_SPIRAM_USE_MALLOC))
#define MEM_ALLOC_EXTRAM(size)         heap_caps_malloc_prefer(size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define MEM_CALLOC_EXTRAM(num, size)   heap_caps_calloc_prefer(num, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#define MEM_REALLOC_EXTRAM(ptr, size)  heap_caps_realloc_prefer(ptr, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL)
#else
#define MEM_ALLOC_EXTRAM(size)         malloc(size)
#define MEM_CALLOC_EXTRAM(num, size)   calloc(num, size)
#define MEM_REALLOC_EXTRAM(ptr, size)  realloc(ptr, size)
#endif

/**
 * @brief User API safe string copy helper
 */
char *app_rmaker_user_api_safe_strdup(const char *str);

/**
 * @brief User API safe string free helper
 */
void app_rmaker_user_api_safe_free(char **ptr);

#ifdef __cplusplus
}
#endif

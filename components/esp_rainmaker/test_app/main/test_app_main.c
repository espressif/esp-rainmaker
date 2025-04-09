/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include <sys/time.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

/* Max bytes a test may allocate before tearDown reports a leak.
 Node Creation holds ~17 KB (work queue, claim, node) until Node Cleanup. */
#define TEST_MEMORY_LEAK_THRESHOLD (20000)

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize the TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create the default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set Wi-Fi mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Preallocate some newlib locks to avoid it from
       registering as memory leaks */

    struct timeval tv = { 0 };
    gettimeofday(&tv, NULL);

    printf("Running console component tests\n");
    unity_run_menu();
}

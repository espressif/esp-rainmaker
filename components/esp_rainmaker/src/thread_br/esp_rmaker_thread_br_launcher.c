/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <esp_check.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_event_base.h>
#include <esp_log.h>
#include <esp_netif.h>
#include <esp_openthread.h>
#include <esp_openthread_border_router.h>
#include <esp_openthread_lock.h>
#include <esp_openthread_netif_glue.h>
#include <esp_openthread_types.h>
#include <esp_openthread_cli.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_thread_br.h>
#include <esp_vfs_dev.h>
#include <esp_vfs_eventfd.h>
#include <esp_wifi_types.h>
#include <mdns.h>
#include <openthread/logging.h>
#include <openthread/thread.h>

#include "esp_rmaker_thread_br_priv.h"

static const char *TAG = "thread_br";

static esp_openthread_platform_config_t s_platform_config;

static void ot_task_worker(void *aContext)
{
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *openthread_netif = esp_netif_new(&cfg);
    assert(openthread_netif != NULL);
    // Initialize the OpenThread stack
    ESP_ERROR_CHECK(esp_openthread_init(&s_platform_config));
    // Initialize border routing features
    esp_openthread_lock_acquire(portMAX_DELAY);
    ESP_ERROR_CHECK(esp_netif_attach(openthread_netif, esp_openthread_netif_glue_init(&s_platform_config)));
#if CONFIG_OPENTHREAD_LOG_LEVEL_DYNAMIC
    (void)otLoggingSetLevel(CONFIG_LOG_DEFAULT_LEVEL);
#endif
    otInstance *instance = esp_openthread_get_instance();
    if (otDatasetIsCommissioned(instance)) {
        (void)otIp6SetEnabled(instance, true);
        (void)otThreadSetEnabled(instance, true);
    }
    esp_openthread_lock_release();

    esp_openthread_launch_mainloop();
    // Clean up
    esp_netif_destroy(openthread_netif);
    esp_openthread_netif_glue_deinit();
    esp_vfs_eventfd_unregister();
    vTaskDelete(NULL);
}

static void thread_br_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                                        void* event_data)
{
    if (event_base == OPENTHREAD_EVENT && event_id == OPENTHREAD_EVENT_ROLE_CHANGED) {
        esp_openthread_role_changed_event_t *role_changed = (esp_openthread_role_changed_event_t *)event_data;
        if (!role_changed) {
            return;
        }
        esp_rmaker_thread_br_report_device_role();
        if (role_changed->current_role == OT_DEVICE_ROLE_ROUTER ||
            role_changed->current_role == OT_DEVICE_ROLE_LEADER) {
            esp_rmaker_thread_br_report_border_agent_id();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        esp_openthread_lock_acquire(portMAX_DELAY);
        // Create link local address for Wi-Fi Station interface
        esp_netif_create_ip6_linklocal(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
        static bool border_router_initialized = false;
        if (!border_router_initialized) {
            ESP_ERROR_CHECK(esp_openthread_border_router_init());
            border_router_initialized = true;
        }
        esp_openthread_lock_release();
    }
}

esp_err_t thread_border_router_start(const esp_openthread_platform_config_t *platform_config)
{
    static bool thread_br_started = false;
    if (thread_br_started) {
        return ESP_OK;
    }
    if (!platform_config) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(&s_platform_config, platform_config, sizeof(esp_openthread_platform_config_t));
    esp_vfs_eventfd_config_t eventfd_config = {
        .max_fds = 4,
    };
    ESP_RETURN_ON_ERROR(esp_vfs_eventfd_register(&eventfd_config), TAG, "Failed to register eventfd");
    esp_openthread_set_backbone_netif(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));
    ESP_ERROR_CHECK(mdns_init());
#define MDNS_MAX_NAME_LEN 64
    char hostname[MDNS_MAX_NAME_LEN];
    if (mdns_hostname_get(hostname) != ESP_OK) {
        // if hostname is not set we will set it with the rainmaker node id.
        ESP_ERROR_CHECK(mdns_hostname_set(esp_rmaker_get_node_id()));
    }
#define THREAD_TASK_STACK_SIZE 8192
    if (xTaskCreate(ot_task_worker, "ot_br", THREAD_TASK_STACK_SIZE, NULL, 5, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to start openthread task for thread br");
        return ESP_FAIL;
    }
    esp_event_handler_register(OPENTHREAD_EVENT, OPENTHREAD_EVENT_ROLE_CHANGED, thread_br_event_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, thread_br_event_handler, NULL);
    thread_br_started = true;
    return ESP_OK;
}

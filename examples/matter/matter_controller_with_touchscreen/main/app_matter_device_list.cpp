/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app_matter_ctrl.h>
#include <app_matter_device_list.h>
#include <app_matter_device_types.h>

#include <app_controller.h>
#include <app_rmaker_matter_controller.h>
#include <app_rmaker_matter_device_list.h>
#include <esp_check.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <sdkconfig.h>

#include <lib/core/CHIPConfig.h>

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "matter_device_list";
static SemaphoreHandle_t s_device_list_mutex;
static bool s_empty_list_update_requested;

static constexpr size_t kMatterControllerMaxActiveDevices = CHIP_CONFIG_CONTROLLER_MAX_ACTIVE_DEVICES;
static constexpr size_t kMatterMaxExchangeContexts = CONFIG_MAX_EXCHANGE_CONTEXTS;
static constexpr size_t kMatterDeviceListMaxDevices = kMatterControllerMaxActiveDevices < kMatterMaxExchangeContexts
                                                      ? kMatterControllerMaxActiveDevices
                                                      : kMatterMaxExchangeContexts;
static_assert(kMatterDeviceListMaxDevices > 0, "Matter controller device-list limit must be non-zero");

matter_device_list_state_t matter_device_list = {0};

static void free_list(matter_device_list_node_t *node)
{
    while (node) {
        matter_device_list_node_t *next = node->next;
        free(node);
        node = next;
    }
}

static matter_device_list_node_t *find_in_list(matter_device_list_node_t *list, uint64_t node_id, uint16_t endpoint_id)
{
    for (matter_device_list_node_t *node = list; node; node = node->next) {
        if (node->node_id == node_id && node->endpoint_id == endpoint_id) {
            return node;
        }
    }
    return NULL;
}

static void *ui_calloc(size_t num, size_t size)
{
    return heap_caps_calloc_prefer(num, size, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM,
                                   MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
}

void matter_device_list_lock(void)
{
    if (!s_device_list_mutex) {
        s_device_list_mutex = xSemaphoreCreateRecursiveMutex();
    }
    ESP_RETURN_VOID_ON_FALSE(s_device_list_mutex, TAG, "Failed to create device-list mutex");
    xSemaphoreTakeRecursive(s_device_list_mutex, portMAX_DELAY);
}

void matter_device_list_unlock(void)
{
    xSemaphoreGiveRecursive(s_device_list_mutex);
}

void matter_device_list_rebuild(const matter_device_t *dev_list)
{
    if (!dev_list) {
        ESP_LOGW(TAG, "No Matter devices returned by device-list update");
        if (app_rmaker_matter_device_list_updatable() && !s_empty_list_update_requested) {
            s_empty_list_update_requested = true;
            app_rmaker_matter_device_list_update();
        }
    }

    matter_device_list_lock();

    matter_device_list_node_t *old_list = matter_device_list.dev_list;
    matter_device_list.dev_list = NULL;
    matter_device_list.device_num = 0;
    matter_device_list.online_num = 0;

    matter_device_list_node_t **tail = &matter_device_list.dev_list;
    size_t skipped_unsupported = 0;
    size_t skipped_limit = 0;
    for (const matter_device_t *dev = dev_list; dev; dev = dev->next) {
        ESP_LOGI(TAG, "Refresh device: node=0x%" PRIx32 "%08" PRIx32 " endpoint_count=%u rainmaker=%d",
                 (uint32_t)(dev->node_id >> 32), (uint32_t)(dev->node_id & 0xFFFFFFFF), dev->endpoint_count,
                 dev->is_rainmaker_device);
        for (uint8_t i = 0; i < dev->endpoint_count; ++i) {
            matter_device_type_t mapped_type = matter_device_type_from_endpoint(&dev->endpoints[i]);
            ESP_LOGI(TAG, "  endpoint=%u device_type_count=%u mapped_type=%d name=%s",
                      dev->endpoints[i].endpoint_id, dev->endpoints[i].device_type_count, mapped_type,
                      dev->device_name);
            if (mapped_type == MATTER_DEVICE_TYPE_UNKNOWN) {
                ++skipped_unsupported;
                continue;
            }
            if (matter_device_list.device_num >= kMatterDeviceListMaxDevices) {
                ++skipped_limit;
                continue;
            }

            matter_device_list_node_t *entry = (matter_device_list_node_t *)ui_calloc(1, sizeof(matter_device_list_node_t));
            if (!entry) {
                continue;
            }

            entry->node_id = dev->node_id;
            entry->endpoint_id = dev->endpoints[i].endpoint_id;
            entry->device_type = mapped_type;
            strlcpy(entry->name, dev->device_name, sizeof(entry->name));
            matter_device_list_node_t *old = find_in_list(old_list, entry->node_id, entry->endpoint_id);
            if (old && old->device_type == entry->device_type) {
                entry->is_online = old->is_online;
                entry->state = old->state;
            }
            *tail = entry;
            tail = &entry->next;
            ++matter_device_list.device_num;
            if (entry->is_online) {
                ++matter_device_list.online_num;
            }
        }
    }

    free_list(old_list);

    matter_device_list_unlock();

    if (skipped_unsupported > 0) {
        ESP_LOGI(TAG, "Skipped %zu unsupported endpoints", skipped_unsupported);
    }
    if (skipped_limit > 0) {
        ESP_LOGW(TAG,
                 "Skipped %zu endpoints due to controller limit %zu (active_devices=%zu exchange_contexts=%zu)",
                 skipped_limit, kMatterDeviceListMaxDevices, kMatterControllerMaxActiveDevices,
                 kMatterMaxExchangeContexts);
    }

    if (dev_list) {
        s_empty_list_update_requested = false;
    }

    ESP_LOGI(TAG, "Rebuild complete: device_num=%zu online_num=%zu", matter_device_list.device_num,
             matter_device_list.online_num);
}

void matter_device_list_fetch(void)
{
    ESP_ERROR_CHECK_WITHOUT_ABORT(app_rmaker_matter_device_list_update());
}

matter_device_list_node_t *matter_device_list_find_locked(uint64_t node_id, uint16_t endpoint_id)
{
    return find_in_list(matter_device_list.dev_list, node_id, endpoint_id);
}

bool matter_device_list_set_online_locked(matter_device_list_node_t *node, bool online)
{
    if (!node || node->is_online == online) {
        return false;
    }
    node->is_online = online;
    if (online) {
        ++matter_device_list.online_num;
    } else if (matter_device_list.online_num > 0) {
        --matter_device_list.online_num;
    }
    return true;
}

bool matter_device_list_set_node_online(uint64_t node_id, bool online)
{
    bool changed = false;
    matter_device_list_lock();
    for (matter_device_list_node_t *node = matter_device_list.dev_list; node; node = node->next) {
        if (node->node_id == node_id) {
            changed |= matter_device_list_set_online_locked(node, online);
        }
    }
    matter_device_list_unlock();
    return changed;
}

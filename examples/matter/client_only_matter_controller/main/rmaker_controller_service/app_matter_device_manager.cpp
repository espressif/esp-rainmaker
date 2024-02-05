/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include <esp_matter_console.h>
#include <controller_rest_apis.h>

#include <app_matter_device_manager.h>
#include <matter_device.h>

#define TAG "mt_dev_mgr"

static TaskHandle_t s_mt_ctl_dev_mgr_task = NULL;
static QueueHandle_t s_mt_ctl_dev_mgr_task_queue = NULL;
static SemaphoreHandle_t s_mt_ctl_dev_mgr_mutex = NULL;

static matter_device_t *s_mt_ctl_dev_list = NULL;
static device_list_update_callback_t s_dev_list_update_cb;

namespace esp_matter {
namespace console {

static engine dev_mgr_console;

static esp_err_t print_dev_list_handler(int argc, char *argv[])
{
     if (!s_mt_ctl_dev_mgr_mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    xSemaphoreTake(s_mt_ctl_dev_mgr_mutex, portMAX_DELAY);
    print_matter_device_list(s_mt_ctl_dev_list);
    xSemaphoreGive(s_mt_ctl_dev_mgr_mutex);
    return ESP_OK;
}

static esp_err_t dev_mgr_dispatch(int argc, char *argv[])
{
    if (argc <= 0) {
        dev_mgr_console.for_each_command(print_description, NULL);
        return ESP_OK;
    }
    return dev_mgr_console.exec_command(argc, argv);
}

esp_err_t ctl_dev_mgr_register_commands()
{
    static const command_t command = {
        .name = "dev_mgr",
        .description = "controller device manager commands. Usage: matter esp dev_mgr <dev_mgr_command>",
        .handler = dev_mgr_dispatch,
    };
    static const command_t dev_mgr_commands[] = {
        {
            .name = "print",
            .description = "print current device list",
            .handler = print_dev_list_handler,
        },
    };
    dev_mgr_console.register_commands(dev_mgr_commands, sizeof(dev_mgr_commands) / sizeof(command_t));
    return add_commands(&command, 1);
}

} // namespace console
} // namespace esp_matter

typedef esp_err_t (*mt_ctl_dev_mgr_action_handler_t)(void *);
typedef struct {
    mt_ctl_dev_mgr_action_handler_t handler;
    void *arg;
} mt_ctl_action_t;

static esp_err_t update_device_list_handler(void *ctx)
{
    if (!s_mt_ctl_dev_mgr_mutex) {
        return ESP_ERR_INVALID_STATE;
    }
    matter_controller_handle_t *handle = (matter_controller_handle_t *)ctx;
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    matter_device_t *tmp = NULL;
    esp_err_t err = fetch_matter_device_list(handle->base_url, handle->access_token, handle->rmaker_group_id, &tmp);
    if (err != ESP_OK) {
        return err;
    }
    xSemaphoreTake(s_mt_ctl_dev_mgr_mutex, portMAX_DELAY);
    if (s_mt_ctl_dev_list) {
        free_matter_device_list(s_mt_ctl_dev_list);
    }
    s_mt_ctl_dev_list = tmp;
    xSemaphoreGive(s_mt_ctl_dev_mgr_mutex);
    if (s_dev_list_update_cb) {
        s_dev_list_update_cb();
    }
    return ESP_OK;
}

esp_err_t update_device_list(matter_controller_handle_t *handle)
{
    if (!s_mt_ctl_dev_mgr_task_queue) {
        ESP_LOGE(TAG, "Failed to update device list as the task queue is not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    mt_ctl_action_t action = {
        .handler = update_device_list_handler,
        .arg = handle,
    };
    if (xQueueSend(s_mt_ctl_dev_mgr_task_queue, &action, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(TAG, "Failed send update device list task");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static matter_device_t *clone_dev_info(matter_device_t *dev)
{
    matter_device_t *ret = (matter_device_t *)malloc(sizeof(matter_device_t));
    if (!ret) {
        ESP_LOGE(TAG, "Failed to allocate memory for matter device struct");
        return NULL;
    }
    memcpy(ret, dev, sizeof(matter_device_t));
    ret->next = NULL;
    return ret;
}

matter_device_t *fetch_device_list()
{
    matter_device_t *ret = NULL;
    xSemaphoreTake(s_mt_ctl_dev_mgr_mutex, portMAX_DELAY);
    matter_device_t *current = s_mt_ctl_dev_list;
    while (current) {
        matter_device_t *tmp = clone_dev_info(current);
        if (!tmp) {
            free_matter_device_list(ret);
            xSemaphoreGive(s_mt_ctl_dev_mgr_mutex);
            return NULL;
        }
        tmp->next = ret;
        ret = tmp;
        current = current->next;
    }
    xSemaphoreGive(s_mt_ctl_dev_mgr_mutex);
    return ret;
}

static void device_mgr_task(void *aContext)
{
    mt_ctl_action_t action;
    while (true) {
        if (xQueueReceive(s_mt_ctl_dev_mgr_task_queue, &action, portMAX_DELAY) == pdTRUE) {
            action.handler(action.arg);
        }
    }
}

esp_err_t init_device_manager(device_list_update_callback_t dev_list_update_cb)
{
    if (s_mt_ctl_dev_mgr_task || s_mt_ctl_dev_mgr_task_queue || s_mt_ctl_dev_mgr_mutex) {
        return ESP_ERR_INVALID_ARG;
    }
    s_mt_ctl_dev_mgr_task_queue = xQueueCreate(8, sizeof(mt_ctl_action_t));
    if (!s_mt_ctl_dev_mgr_task_queue) {
        ESP_LOGI(TAG, "Failed to create dev_mgr_task_queue");
        return ESP_ERR_NO_MEM;
    }
    s_mt_ctl_dev_mgr_mutex = xSemaphoreCreateRecursiveMutex();
    if (!s_mt_ctl_dev_mgr_mutex) {
        ESP_LOGE(TAG, "Failed to create device mgr lock");
        vQueueDelete(s_mt_ctl_dev_mgr_task_queue);
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(device_mgr_task, "device_mgr", 4096, NULL, 5, &s_mt_ctl_dev_mgr_task) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create device mgr task");
        vQueueDelete(s_mt_ctl_dev_mgr_task_queue);
        vSemaphoreDelete(s_mt_ctl_dev_mgr_mutex);
        return ESP_ERR_NO_MEM;
    }
    s_dev_list_update_cb = dev_list_update_cb;
    return ESP_OK;
}

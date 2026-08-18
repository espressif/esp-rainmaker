/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <app_controller_console.h>

#include <app_controller.h>
#include <app_rmaker_matter_controller.h>
#include <esp_console.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <nvs_flash.h>

static const char *TAG = "app_controller_console";

static void erase_partition_by_label(const char *label, esp_partition_type_t type, esp_partition_subtype_t subtype)
{
    const esp_partition_t *partition = esp_partition_find_first(type, subtype, label);
    if (partition) {
        ESP_LOGW(TAG, "Factory reset: erasing %s partition", label);
        ESP_ERROR_CHECK_WITHOUT_ABORT(esp_partition_erase_range(partition, 0, partition->size));
    } else {
        ESP_LOGW(TAG, "%s partition not found", label);
    }
}

static void erase_factory_partitions(void)
{
    erase_partition_by_label("esp_secure_cert", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY);
    erase_partition_by_label("fctry", ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS);
}

static int app_controller_list_devices_handler(int argc, char **argv)
{
    matter_device_t *device_list = app_controller_device_list_copy();
    if (!device_list) {
        ESP_LOGE(TAG, "Failed to get matter device list");
        return ESP_FAIL;
    }

    app_rmaker_device_list_print(device_list);
    app_rmaker_device_list_copy_destroy(device_list);
    return ESP_OK;
}

static int app_controller_factory_reset_handler(int argc, char **argv)
{
    ESP_LOGW(TAG, "Factory reset requested");
    nvs_flash_deinit();
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
    erase_factory_partitions();
    esp_restart();
    return ESP_OK;
}

void app_controller_register_commands(void)
{
    const esp_console_cmd_t list_devices_cmd = {
        .command = "matter_device_list",
        .help = "Print the RainMaker Matter device list",
        .hint = NULL,
        .func = &app_controller_list_devices_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&list_devices_cmd));

    const esp_console_cmd_t factory_reset_cmd = {
        .command = "matter_factory_reset",
        .help = "Factory reset the device",
        .hint = NULL,
        .func = &app_controller_factory_reset_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&factory_reset_cmd));
}

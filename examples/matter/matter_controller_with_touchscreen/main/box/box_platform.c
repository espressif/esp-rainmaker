#include "box_platform.h"

#include <string.h>

#include "dev_display_lcd.h"
#include "dev_lcd_touch.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "box_platform";
static lv_display_t *s_display;
static bool initialized = false;

esp_err_t box_platform_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    esp_lv_adapter_config_t lvgl_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();

    ESP_RETURN_ON_ERROR(esp_board_manager_init(), TAG, "Failed to init board manager");
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&lvgl_cfg), TAG, "Failed to init LVGL");

    void *lcd_handle = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &lcd_handle), TAG,
                        "Failed to get LCD handle");
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_cfg), TAG,
                        "Failed to get LCD config");

    dev_display_lcd_handles_t *lcd_handles = (dev_display_lcd_handles_t *)lcd_handle;
    esp_lv_adapter_rotation_t rotation = ESP_LV_ADAPTER_ROTATE_0;
    if (lcd_cfg->swap_xy) {
        rotation = lcd_cfg->mirror_x ? ESP_LV_ADAPTER_ROTATE_90 : ESP_LV_ADAPTER_ROTATE_270;
    } else if (lcd_cfg->mirror_x && lcd_cfg->mirror_y) {
        rotation = ESP_LV_ADAPTER_ROTATE_180;
    }

    if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_SPI) == 0 ||
            strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_PARLIO) == 0) {
        esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_SPI_WITH_PSRAM_DEFAULT_CONFIG(
                                                          lcd_handles->panel_handle, lcd_handles->io_handle,
                                                          lcd_cfg->lcd_width, lcd_cfg->lcd_height, rotation);
        s_display = esp_lv_adapter_register_display(&display_cfg);
    }
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    else if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        esp_lv_adapter_display_config_t display_cfg = ESP_LV_ADAPTER_DISPLAY_MIPI_DEFAULT_CONFIG(
                                                          lcd_handles->panel_handle, lcd_handles->io_handle,
                                                          lcd_cfg->lcd_width, lcd_cfg->lcd_height, rotation);
        display_cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE;
        s_display = esp_lv_adapter_register_display(&display_cfg);
    }
#endif
    else {
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, TAG, "Unsupported LCD subtype");
    }
    ESP_RETURN_ON_FALSE(s_display != NULL, ESP_FAIL, TAG, "Failed to add LCD display to LVGL");

#if CONFIG_ESP_BOARD_DEV_LCD_TOUCH_SUB_I2C_SUPPORT
    void *touch_handle = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, &touch_handle) == ESP_OK && touch_handle) {
        dev_lcd_touch_handles_t *touch_handles = (dev_lcd_touch_handles_t *)touch_handle;
        esp_lv_adapter_touch_config_t touch_cfg = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_display, touch_handles->touch_handle);
        if (!esp_lv_adapter_register_touch(&touch_cfg)) {
            ESP_LOGW(TAG, "Failed to add LCD touch to LVGL");
        }
    }
#endif

    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "Failed to start LVGL");

    initialized = true;
    return ESP_OK;
}

const char *box_platform_get_name(void)
{
    if (!initialized || g_esp_board_info.name == NULL) {
        return "unknown";
    }

    return g_esp_board_info.name;
}

static void erase_partition_by_label(const char *label)
{
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_ANY,
                                                                ESP_PARTITION_SUBTYPE_ANY, label);
    if (!partition) {
        ESP_LOGW(TAG, "Factory reset: partition '%s' not found", label);
        return;
    }

    esp_err_t err = esp_partition_erase_range(partition, 0, partition->size);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Factory reset: failed to erase partition '%s': %s", label, esp_err_to_name(err));
    }
}

void box_platform_factory_reset(void)
{
    ESP_LOGW(TAG, "Factory reset: erasing Wi-Fi, NVS, factory data, and secure cert partitions");
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_restore());
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_deinit());
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
    erase_partition_by_label("fctry");
    erase_partition_by_label("esp_secure_cert");
    esp_restart();
}

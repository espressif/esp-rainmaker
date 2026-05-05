#include "box_platform.h"

#include <string.h>

#include "dev_display_lcd.h"
#include "dev_lcd_touch_i2c.h"
#include "esp_board_manager.h"
#include "esp_board_manager_defs.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "box_platform";
static lv_disp_t *s_display;
static bool initialized = false;

esp_err_t box_platform_init(void)
{
    if (initialized) {
        return ESP_OK;
    }

    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,
        .task_stack = 8192,
        .task_affinity = 1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };

    ESP_RETURN_ON_ERROR(esp_board_manager_init(), TAG, "Failed to init board manager");
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "Failed to init LVGL");

    void *lcd_handle = NULL;
    dev_display_lcd_config_t *lcd_cfg = NULL;
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &lcd_handle), TAG,
                        "Failed to get LCD handle");
    ESP_RETURN_ON_ERROR(esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, (void **)&lcd_cfg), TAG,
                        "Failed to get LCD config");

    dev_display_lcd_handles_t *lcd_handles = (dev_display_lcd_handles_t *)lcd_handle;
    lvgl_port_display_cfg_t display_cfg = {
        .io_handle = lcd_handles->io_handle,
        .panel_handle = lcd_handles->panel_handle,
        .buffer_size = lcd_cfg->lcd_width * lcd_cfg->lcd_height,
        .double_buffer = true,
        .hres = lcd_cfg->lcd_width,
        .vres = lcd_cfg->lcd_height,
        .monochrome = false,
        .rotation = {
            .swap_xy = lcd_cfg->swap_xy,
            .mirror_x = lcd_cfg->mirror_x,
            .mirror_y = lcd_cfg->mirror_y,
        },
        .flags = {
            .buff_spiram = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
        },
    };

    if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_SPI) == 0 ||
            strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_PARLIO) == 0) {
        s_display = lvgl_port_add_disp(&display_cfg);
    }
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    else if (strcmp(lcd_cfg->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        lvgl_port_display_dsi_cfg_t dsi_cfg = {
            .flags = {
                .avoid_tearing = false,
            },
        };
#if LVGL_VERSION_MAJOR >= 9
        display_cfg.flags.swap_bytes = false;
#endif
        s_display = lvgl_port_add_disp_dsi(&display_cfg, &dsi_cfg);
    }
#endif
    else {
        ESP_RETURN_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, TAG, "Unsupported LCD subtype");
    }
    ESP_RETURN_ON_FALSE(s_display != NULL, ESP_FAIL, TAG, "Failed to add LCD display to LVGL");

#if CONFIG_ESP_BOARD_DEV_LCD_TOUCH_I2C_SUPPORT
    void *touch_handle = NULL;
    if (esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, &touch_handle) == ESP_OK && touch_handle) {
        dev_lcd_touch_i2c_handles_t *touch_handles = (dev_lcd_touch_i2c_handles_t *)touch_handle;
        lvgl_port_touch_cfg_t touch_cfg = {
            .disp = s_display,
            .handle = touch_handles->touch_handle,
        };
        if (!lvgl_port_add_touch(&touch_cfg)) {
            ESP_LOGW(TAG, "Failed to add LCD touch to LVGL");
        }
    }
#endif

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

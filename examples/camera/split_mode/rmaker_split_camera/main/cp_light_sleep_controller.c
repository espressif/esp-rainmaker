/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cp_light_sleep_controller.h"
#include "esp_log.h"
#include "esp_check.h"
#include "sdkconfig.h"

#ifdef CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_ENABLE

#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char TAG[] = "cp_light_sleep";

#if CONFIG_EXAMPLE_BOOT_DIAG
/* Diagnostics for the reset seen at the first light-sleep entry.
 * RTC_NOINIT survives light sleep and a software reset, and is cleared only
 * when the chip loses power. Its value after a reboot says whether the rail
 * actually dropped or the ROM merely reported rst:0x1. */
#include <inttypes.h>
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_system.h"

#define CP_LS_CANARY 0x5AFE51EEu
RTC_NOINIT_ATTR static uint32_t s_ls_canary;
RTC_NOINIT_ATTR static uint32_t s_ls_entries;

static void cp_ls_report_boot(void)
{
    if (s_ls_canary == CP_LS_CANARY) {
        ESP_LOGW(TAG, "DIAG: RTC canary SURVIVED, entries=%" PRIu32
                 " -> core kept power; rst:0x1 is not a real supply loss",
                 ++s_ls_entries);
    } else {
        ESP_LOGW(TAG, "DIAG: RTC canary LOST (0x%08" PRIx32 ") -> chip lost power",
                 s_ls_canary);
        s_ls_entries = 0;
    }
    s_ls_canary = CP_LS_CANARY;
    ESP_LOGW(TAG, "DIAG: reset reason=%d wakeup cause=%d",
             (int) esp_reset_reason(), (int) esp_sleep_get_wakeup_cause());
    esp_pm_config_t c = {0};
    if (esp_pm_get_configuration(&c) == ESP_OK) {
        ESP_LOGW(TAG, "DIAG: pm max=%d min=%d light_sleep=%d",
                 c.max_freq_mhz, c.min_freq_mhz, (int) c.light_sleep_enable);
    }
}
#endif /* CONFIG_EXAMPLE_BOOT_DIAG */



/* Held together while the host is awake: no naps, and no clock drop. */
static esp_pm_lock_handle_t pm_lock = NULL;        /* NO_LIGHT_SLEEP */
static esp_pm_lock_handle_t freq_lock = NULL;      /* CPU_FREQ_MAX   */
static bool pm_lock_acquired = false;
static bool pm_configured = false;
/* start() and stop() run from both the host power-save worker and the wake
 * path, so the lock state changes under this mutex. */
static SemaphoreHandle_t s_state_mutex;

esp_err_t cp_light_sleep_controller_init(void)
{
#if CONFIG_EXAMPLE_BOOT_DIAG
    cp_ls_report_boot();
#endif

	if (pm_configured) {
		ESP_LOGW(TAG, "Light sleep already initialized");
		return ESP_OK;
	}

	ESP_LOGI(TAG, "Initializing light sleep power management");

	/* Held while the host is awake, released for its sleep. pm_lock is
	 * required: no uart/SDIO wake source is armed, so a nap loses the frame. */
	esp_err_t ret = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "slave_no_ls_lock", &pm_lock);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create no-light-sleep lock: %s", esp_err_to_name(ret));
		return ret;
	}
	ret = esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "slave_freq_lock", &freq_lock);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create frequency lock: %s", esp_err_to_name(ret));
		esp_pm_lock_delete(pm_lock);
		pm_lock = NULL;
		return ret;
	}
	if (s_state_mutex == NULL) {
		s_state_mutex = xSemaphoreCreateMutex();
		if (s_state_mutex == NULL) {
			esp_pm_lock_delete(pm_lock);
			pm_lock = NULL;
			esp_pm_lock_delete(freq_lock);
			freq_lock = NULL;
			return ESP_ERR_NO_MEM;
		}
	}

	/* Hold both locks before auto light sleep is switched on, so the CP
	 * cannot nap between esp_pm_configure() and the first stop(). */
	ret = esp_pm_lock_acquire(pm_lock);
	if (ret == ESP_OK) {
		ret = esp_pm_lock_acquire(freq_lock);
		if (ret != ESP_OK) {
			esp_pm_lock_release(pm_lock);
		}
	}
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to acquire PM locks: %s", esp_err_to_name(ret));
		esp_pm_lock_delete(pm_lock);
		pm_lock = NULL;
		esp_pm_lock_delete(freq_lock);
		freq_lock = NULL;
		return ret;
	}
	pm_lock_acquired = true;

	esp_pm_config_t pm_config = {
		.max_freq_mhz       = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
		.min_freq_mhz       = CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_MIN_FREQ_MHZ,
		.light_sleep_enable = true,
	};

	ret = esp_pm_configure(&pm_config);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to configure PM: %s", esp_err_to_name(ret));
		esp_pm_lock_release(pm_lock);
		esp_pm_lock_release(freq_lock);
		pm_lock_acquired = false;
		esp_pm_lock_delete(pm_lock);
		pm_lock = NULL;
		esp_pm_lock_delete(freq_lock);
		freq_lock = NULL;
		return ret;
	}

	ESP_LOGI(TAG, "PM configured: max=%d MHz, min=%d MHz, light_sleep=enabled",
	         pm_config.max_freq_mhz, pm_config.min_freq_mhz);

#if defined(CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP) && defined(CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_PERIPHERAL_POWERDOWN)
	ESP_LOGI(TAG, "  Peripheral powerdown: ENABLED (UART console disabled in sleep)");
#else
	ESP_LOGI(TAG, "  Peripheral powerdown: DISABLED (UART console available)");
#endif

	pm_configured = true;
	ESP_LOGI(TAG, "Light sleep DISABLED");

	ESP_LOGI(TAG, "Light sleep initialized successfully");
	return ESP_OK;
}

esp_err_t cp_light_sleep_controller_start(void)
{
	if (!pm_configured || !pm_lock) {
		ESP_LOGE(TAG, "Light sleep not initialized");
		return ESP_ERR_INVALID_STATE;
	}
	xSemaphoreTake(s_state_mutex, portMAX_DELAY);
	if (!pm_lock_acquired) {
		xSemaphoreGive(s_state_mutex);
		return ESP_OK;
	}
	esp_err_t ret = esp_pm_lock_release(pm_lock);
	esp_err_t fret = freq_lock ? esp_pm_lock_release(freq_lock) : ESP_OK;
	if (ret == ESP_OK && fret == ESP_OK) {
		pm_lock_acquired = false;
		ESP_EARLY_LOGI(TAG, "Light sleep ENABLED");
	} else {
		ESP_EARLY_LOGE(TAG, "Failed to release PM locks: %s / %s",
		               esp_err_to_name(ret), esp_err_to_name(fret));
	}
	xSemaphoreGive(s_state_mutex);
	return (ret != ESP_OK) ? ret : fret;
}

esp_err_t cp_light_sleep_controller_stop(void)
{
	if (!pm_configured || !pm_lock) {
		ESP_LOGE(TAG, "Light sleep not initialized");
		return ESP_ERR_INVALID_STATE;
	}
	xSemaphoreTake(s_state_mutex, portMAX_DELAY);
	if (pm_lock_acquired) {
		xSemaphoreGive(s_state_mutex);
		return ESP_OK;
	}
	esp_err_t ret = esp_pm_lock_acquire(pm_lock);
	esp_err_t fret = freq_lock ? esp_pm_lock_acquire(freq_lock) : ESP_OK;
	if (ret == ESP_OK && fret == ESP_OK) {
		pm_lock_acquired = true;
		ESP_LOGI(TAG, "Light sleep DISABLED");
	} else {
		ESP_LOGE(TAG, "Failed to acquire PM locks: %s / %s",
		         esp_err_to_name(ret), esp_err_to_name(fret));
	}
	xSemaphoreGive(s_state_mutex);
	return (ret != ESP_OK) ? ret : fret;
}

esp_err_t cp_light_sleep_controller_is_configured(void)
{
	return pm_configured ? ESP_OK : ESP_FAIL;
}

esp_err_t cp_light_sleep_controller_deinit(void)
{
	if (!pm_configured) {
		return ESP_OK;
	}
	/* esp_pm_lock_delete() refuses a held lock, so release them first. */
	xSemaphoreTake(s_state_mutex, portMAX_DELAY);
	if (pm_lock_acquired) {
		esp_pm_lock_release(pm_lock);
		if (freq_lock) esp_pm_lock_release(freq_lock);
	}
	if (pm_lock) {
		esp_pm_lock_delete(pm_lock);
		pm_lock = NULL;
	}
	if (freq_lock) {
		esp_pm_lock_delete(freq_lock);
		freq_lock = NULL;
	}
	pm_lock_acquired = false;
	pm_configured = false;
	xSemaphoreGive(s_state_mutex);
	return ESP_OK;
}

#else

esp_err_t cp_light_sleep_controller_init(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t cp_light_sleep_controller_start(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t cp_light_sleep_controller_stop(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t cp_light_sleep_controller_is_configured(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t cp_light_sleep_controller_deinit(void) { return ESP_ERR_NOT_SUPPORTED; }

#endif /* CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_ENABLE */

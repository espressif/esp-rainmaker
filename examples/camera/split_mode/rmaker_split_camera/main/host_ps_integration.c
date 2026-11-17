/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stddef.h>
#include "host_ps_integration.h"
#include "sdkconfig.h"

static void (*s_cli_register_cb)(void);

void host_ps_integration_set_cli_register_cb(void (*cb)(void))
{
	s_cli_register_cb = cb;
}

#if CONFIG_ESP_HOSTED_CP_FEAT_HOST_PS

#include "cp_light_sleep_controller.h"
#include "eh_cp_feat_host_ps.h"
#include "eh_cp_feat_host_ps_apis.h"
#include "eh_cp_cli.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#if CONFIG_HEAP_TRACING
#define MEMORY_LEAK_CHECKING 1
#else
#define MEMORY_LEAK_CHECKING 0
#endif

#include "eh_cp_utils.h"
#if MEMORY_LEAK_CHECKING
#include "eh_cp_feat_debug.h"
#endif

static const char *TAG = "host_ps_integration";
#if MEMORY_LEAK_CHECKING
static uint32_t s_cycle;
#endif

static inline void optional_debug_log_heap_snapshot(const char *stage)
{
#if MEMORY_LEAK_CHECKING
	eh_cp_utils_log_mem_stats(TAG, stage);
#else
	(void)stage;
#endif
}

static inline void optional_debug_cycle_begin(void)
{
#if MEMORY_LEAK_CHECKING
	s_cycle++;
	eh_cp_feat_debug_heap_cycle_begin(s_cycle);
#endif
}

static inline void optional_debug_cycle_end(void)
{
#if MEMORY_LEAK_CHECKING
	eh_cp_feat_debug_heap_cycle_end(s_cycle);
#endif
}

#if defined(CONFIG_PM_POWER_DOWN_PERIPHERAL_IN_LIGHT_SLEEP) && \
    defined(CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_PERIPHERAL_POWERDOWN) && \
    defined(CONFIG_ESP_HOSTED_CP_FEAT_CLI)
#define CLI_STOPS_IN_LIGHT_SLEEP 1

/* The CLI stops before the UART powers down in light sleep, and starts again
 * when the host wakes. One task does both, in order. The host power-save
 * callbacks only set s_cli_wanted and notify it, because they run on
 * esp_hosted's ps_alert_task, partly under its transition lock. A console
 * command that wakes the host would wait for that lock while the callback
 * waits for the console to stop. The task holds a NO_LIGHT_SLEEP lock while
 * the CLI runs, so the chip sleeps only after the CLI has stopped. */
#define CLI_TASK_STACK  3072
#define CLI_TASK_PRIO   3

static TaskHandle_t s_cli_task;
static esp_pm_lock_handle_t s_cli_pm_lock;
static volatile bool s_cli_wanted = true;

static void cli_task(void *arg)
{
	bool running = true;    /* the app started the CLI at boot */

	(void)arg;
	for (;;) {
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		/* Read the wanted state once per pass: a sleep and wake between two reads
		 * would otherwise start a CLI that is already running. */
		bool want;
		while (running != (want = s_cli_wanted)) {
			if (want) {
				esp_pm_lock_acquire(s_cli_pm_lock);
				ESP_LOGI(TAG, "Restarting CLI (UART powered up)");
				(void)eh_cp_feat_cli_start(NULL);
				if (s_cli_register_cb != NULL) {
					s_cli_register_cb();
				}
				running = true;
			} else {
				ESP_LOGI(TAG, "Stopping CLI (UART powering down)");
				eh_cp_feat_cli_stop();
				running = false;
				esp_pm_lock_release(s_cli_pm_lock);
			}
			ESP_LOGD(TAG, "CLI task stack left: %u B", (unsigned)uxTaskGetStackHighWaterMark(NULL));
		}
	}
}

static esp_err_t cli_task_init(void)
{
	if (s_cli_task != NULL) {
		return ESP_OK;
	}
	esp_err_t ret = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "cli", &s_cli_pm_lock);
	if (ret != ESP_OK) {
		return ret;
	}
	esp_pm_lock_acquire(s_cli_pm_lock);     /* the CLI is running */
	if (xTaskCreate(cli_task, "cp_cli", CLI_TASK_STACK, NULL, CLI_TASK_PRIO, &s_cli_task) != pdPASS) {
		esp_pm_lock_release(s_cli_pm_lock);
		esp_pm_lock_delete(s_cli_pm_lock);
		s_cli_pm_lock = NULL;
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}

static inline void set_cli_wanted(bool wanted)
{
	if (s_cli_task != NULL) {
		s_cli_wanted = wanted;
		xTaskNotifyGive(s_cli_task);
	}
}
#else
#define CLI_STOPS_IN_LIGHT_SLEEP 0
#endif

static inline void stop_cli_for_light_sleep(void)
{
#if CLI_STOPS_IN_LIGHT_SLEEP
	set_cli_wanted(false);
#endif
}

static inline void start_cli_after_light_sleep(void)
{
#if CLI_STOPS_IN_LIGHT_SLEEP
	set_cli_wanted(true);
#endif
}

/* Optional host power-save counters (CONFIG_APP_CP_PS_STATS, default n). */
#if CONFIG_APP_CP_PS_STATS
	static uint32_t s_ps_sleeps;   /* host announced power-save entry */
	static uint32_t s_ps_wakes;    /* host came back and the link is serviceable */

	#define APP_CP_PS_STATS_SLEEP()  do { s_ps_sleeps++; } while (0)
	#define APP_CP_PS_STATS_WAKE()   do { s_ps_wakes++; } while (0)

	/* RAM-only counters; first-cycle marker helps identify a CP reboot. */
	#define APP_CP_PS_STATS_LOG(at) \
		ESP_EARLY_LOGI(TAG, "[ps_stats] %s sleeps=%u wakes=%u%s", \
		               (at), (unsigned)s_ps_sleeps, (unsigned)s_ps_wakes, \
		               (s_ps_sleeps <= 1u && s_ps_wakes == 0u) \
		                   ? " (first cycle since this CP booted)" : "")
#else
	#define APP_CP_PS_STATS_SLEEP()  do { } while (0)
	#define APP_CP_PS_STATS_WAKE()   do { } while (0)
	#define APP_CP_PS_STATS_LOG(at)  do { } while (0)
#endif

static void on_prepare_cb(void)
{
	ESP_EARLY_LOGI(TAG, "==> Host preparing to enter power save");
	eh_cp_utils_log_mem_stats(TAG, "ps_cycle");
	APP_CP_PS_STATS_SLEEP();
	APP_CP_PS_STATS_LOG("host->sleep");
	optional_debug_log_heap_snapshot("on_prepare");
	optional_debug_cycle_begin();
}

static void on_ready_cb(void)
{
	ESP_EARLY_LOGI(TAG, "==> Host power save active - entering light sleep");
	optional_debug_log_heap_snapshot("on_ready_before");

#ifdef CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_ENABLE
	stop_cli_for_light_sleep();
	/* While the host sleeps, esp_hosted logs two INFO lines for each host-bound
	 * packet it drops (about one a second). At 115200 baud each costs
	 * milliseconds awake. Keep the warnings. */
	esp_log_level_set("ehcp_host_ps", ESP_LOG_WARN);
	(void)cp_light_sleep_controller_start();
#else
	ESP_EARLY_LOGW(TAG, "Light sleep not enabled in menuconfig");
#endif

	optional_debug_log_heap_snapshot("on_ready_after");
}

static void off_prepare_cb(void)
{
	optional_debug_log_heap_snapshot("off_prepare_before");
#ifdef CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_ENABLE
	(void)cp_light_sleep_controller_stop();
#endif
	optional_debug_log_heap_snapshot("off_prepare_after");
}

static void off_ready_cb(void)
{
	ESP_EARLY_LOGI(TAG, "==> Host power save off - device fully ready");
	APP_CP_PS_STATS_WAKE();
	APP_CP_PS_STATS_LOG("host->awake");
	optional_debug_log_heap_snapshot("off_ready_before");

	start_cli_after_light_sleep();
	esp_log_level_set("ehcp_host_ps", ESP_LOG_INFO);
	optional_debug_cycle_end();

	optional_debug_log_heap_snapshot("off_ready_after");
}

esp_err_t host_ps_integration_init(void)
{
	ESP_LOGI(TAG, "Initializing host-deep-sleep ↔ CP-light-sleep integration");

#if MEMORY_LEAK_CHECKING
	eh_cp_feat_debug_heap_log("example_init");
#endif

	const host_power_save_callbacks_t cbs = {
		.host_power_save_on_prepare_cb  = on_prepare_cb,
		.host_power_save_on_ready_cb    = on_ready_cb,
		.host_power_save_off_prepare_cb = off_prepare_cb,
		.host_power_save_off_ready_cb   = off_ready_cb,
	};

#if CLI_STOPS_IN_LIGHT_SLEEP
	esp_err_t cli_ret = cli_task_init();
	if (cli_ret != ESP_OK) {
		ESP_LOGE(TAG, "CLI task init failed: %s", esp_err_to_name(cli_ret));
		return cli_ret;
	}
#endif

	esp_err_t ret = eh_cp_feat_host_ps_cp__enable(&cbs);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "host_ps enable failed: %s", esp_err_to_name(ret));
		return ret;
	}

#ifdef CONFIG_ESP_HOSTED_CP_LIGHT_SLEEP_ENABLE
	ret = cp_light_sleep_controller_init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Light sleep init failed: %s", esp_err_to_name(ret));
		ESP_LOGE(TAG, "Check menuconfig: PM_ENABLE and FREERTOS_USE_TICKLESS_IDLE");
		return ret;
	}
#else
	ESP_LOGW(TAG, "Light sleep not enabled in menuconfig — host_ps callbacks still active");
#endif

	ESP_LOGI(TAG, "Integration ready: host sleep ⇄ CP light sleep");
	return ESP_OK;
}

#else

/* esp_hosted did not build eh_cp_feat_host_ps for this target, so there is no
 * host power save to hook into. */
esp_err_t host_ps_integration_init(void) { return ESP_ERR_NOT_SUPPORTED; }

#endif /* CONFIG_ESP_HOSTED_CP_FEAT_HOST_PS */

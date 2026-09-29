/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <esp_console.h>
#include <esp_log.h>
#if CONFIG_ESP_HOSTED_CP_FEAT_HOST_PS
#include "eh_cp_feat_host_ps_apis.h"
#endif

static const char *TAG = "sleep_cmd";

static int wakeup_cli_handler(int argc, char *argv[])
{
#if CONFIG_ESP_HOSTED_CP_FEAT_HOST_PS
    ESP_LOGI(TAG, "Asking P4 to wake-up...");
    eh_cp_feat_host_ps_wakeup_host(500);
#endif
    return 0;
}

static esp_console_cmd_t sleep_cmds[] = {
    {
        .command = "wake-up",
        .help = "Wake-up P4 from deep sleep",
        .func = wakeup_cli_handler,
    }
};

int sleep_command_register_cli()
{
    int cmds_num = sizeof(sleep_cmds) / sizeof(esp_console_cmd_t);
    for (int i = 0; i < cmds_num; i++) {
        ESP_LOGI(TAG, "Registering command: %s", sleep_cmds[i].command);
        esp_console_cmd_register(&sleep_cmds[i]);
    }
    return 0;
}

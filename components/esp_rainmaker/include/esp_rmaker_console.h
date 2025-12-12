// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/** Initialize console
 *
 * Initializes serial console and adds basic commands.
 *
 * @return ESP_OK on success.
 * @return error in case of failures.
 *
 * @note This function does console initialization and registers the rmaker console commands.
 *  If you wish to initialize the console yourself, you can call esp_rmaker_console_register_commands after console init
 */
esp_err_t esp_rmaker_console_init(void);

/**
 * @brief Register rmaker console commands
 *
 * @return void
 *
 * @note This function must be called after console initialization
 *  If you wish to register commands yourself, you can call this function after console initialization
 *  Should not be called if esp_rmaker_console_init is called which does this implicitly
 */
void esp_rmaker_register_commands(void);

/* Reference for adding custom console commands:
#include <esp_console.h>

static int command_console_handler(int argc, char *argv[])
{
    // Command code here
}

static void register_console_command()
{
    const esp_console_cmd_t cmd = {
        .command = "<command_name>",
        .help = "<help_details>",
        .func = &command_console_handler,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
*/

#ifdef __cplusplus
}
#endif

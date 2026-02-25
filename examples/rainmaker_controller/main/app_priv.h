/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* Private app declarations for rainmaker_controller */

#pragma once

#include <stdint.h>

/** @brief Initialize the board driver (e.g. button for reset) */
void app_driver_init(void);

/** @brief Initialize the CLI commands and start REPL */
void app_cli_command_init(void);

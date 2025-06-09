/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register sleep-related CLI commands
 *
 * This function registers CLI commands for power management, such as
 * wake-up commands for split mode architectures.
 *
 * @return 0 on success, non-zero on failure
 */
int sleep_command_register_cli(void);

#ifdef __cplusplus
}
#endif

/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef HOST_PS_INTEGRATION_H
#define HOST_PS_INTEGRATION_H

#include "esp_err.h"

/* Registers the four host_ps callbacks and brings up the light-sleep driver.
 * After this returns, host power save drives CP light sleep. */
esp_err_t host_ps_integration_init(void);

/* Restarting the CLI on a host wake wipes every console command, and
 * esp_hosted adds back only its own. cb runs after that restart, so the
 * application can register its commands again. */
void host_ps_integration_set_cli_register_cb(void (*cb)(void));

#endif /* HOST_PS_INTEGRATION_H */

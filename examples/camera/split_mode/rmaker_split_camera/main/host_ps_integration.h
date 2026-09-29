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

#endif /* HOST_PS_INTEGRATION_H */

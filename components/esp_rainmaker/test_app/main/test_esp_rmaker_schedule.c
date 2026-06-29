/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "esp_rmaker_core.h"
#include "esp_rmaker_schedule.h"


TEST_CASE("Schedule enable with node", "[esp_rmaker_schedule]")
{
    /* Schedule service requires node to exist (from Node Creation test) */
    TEST_ASSERT_NOT_NULL(esp_rmaker_get_node());
    esp_err_t err = esp_rmaker_schedule_enable();
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

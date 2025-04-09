/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include "unity.h"
#include "esp_event.h"
#include "esp_ota_ops.h"
#include "esp_rmaker_core.h"
#include "esp_rmaker_ota.h"

TEST_CASE("OTA enable with node and default config returns ESP_OK", "[esp_rmaker_ota]")
{
    /* OTA enable requires node from core (Node Creation test must have run first) */
    if (esp_rmaker_get_node() == NULL) {
        TEST_IGNORE_MESSAGE("Requires node; run after Node Creation test");
        return;
    }
    esp_err_t err = esp_rmaker_ota_enable(NULL, OTA_USING_PARAMS);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("OTA report_status with NULL handle returns ESP_FAIL", "[esp_rmaker_ota]")
{
    esp_err_t err = esp_rmaker_ota_report_status(NULL, OTA_STATUS_IN_PROGRESS, "test");
    TEST_ASSERT_EQUAL(ESP_FAIL, err);
}


TEST_CASE("OTA mark_valid when not in OTA returns error", "[esp_rmaker_ota]")
{
    esp_err_t err = esp_rmaker_ota_mark_valid();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, err);
}

TEST_CASE("OTA mark_invalid when not pending verify returns INVALID_STATE", "[esp_rmaker_ota]")
{
    esp_err_t err = esp_rmaker_ota_mark_invalid();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, err);
}

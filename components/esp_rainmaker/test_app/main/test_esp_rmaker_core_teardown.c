/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "esp_rmaker_core.h"

TEST_CASE("ESP RainMaker Node Cleanup", "[esp_rmaker_core]")
{
    const esp_rmaker_node_t *node = esp_rmaker_get_node();
    if (node == NULL) {
        TEST_IGNORE_MESSAGE("No node to cleanup (core Node Creation may not have run)");
        return;
    }
    esp_err_t err = esp_rmaker_node_deinit(node);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

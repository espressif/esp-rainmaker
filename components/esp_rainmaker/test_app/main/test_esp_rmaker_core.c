/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_rmaker_core.h"
#include "esp_rmaker_standard_types.h"
#include "esp_rmaker_standard_params.h"
#include "esp_rmaker_standard_devices.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "esp_rmaker_core_test";
static SemaphoreHandle_t write_cb_semaphore;
static esp_rmaker_device_t *test_device = NULL;
static esp_rmaker_param_t *test_param = NULL;
static esp_rmaker_node_t *test_node = NULL;
static esp_rmaker_param_val_t test_param_val;
static bool param_update_called = false;

// Callback when a parameter of the device is updated
static esp_err_t write_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param,
                          const esp_rmaker_param_val_t val, void *priv_data, esp_rmaker_write_ctx_t *ctx)
{
    ESP_LOGI(TAG, "Parameter write callback called");

    // Use device and param pointers to identify the device and param
    TEST_ASSERT_EQUAL_PTR(test_device, device);
    TEST_ASSERT_EQUAL_PTR(test_param, param);

    // Remember the updated value
    test_param_val = val;
    param_update_called = true;

    if (write_cb_semaphore) {
        xSemaphoreGive(write_cb_semaphore);
    }
    return ESP_OK;
}

// Callback to read values for parameters
static esp_err_t read_cb(const esp_rmaker_device_t *device, const esp_rmaker_param_t *param, void *priv_data, esp_rmaker_read_ctx_t *ctx)
{
    ESP_LOGI(TAG, "Parameter read callback called");

    // Use device and param pointers to identify the device and param
    TEST_ASSERT_EQUAL_PTR(test_device, device);
    TEST_ASSERT_EQUAL_PTR(test_param, param);

    // Handle the value as needed
    return ESP_OK;
}

TEST_CASE("ESP RainMaker Node Creation", "[esp_rmaker_core]")
{
    esp_rmaker_config_t rmaker_config = { 0 };

    // Create a RainMaker node
    test_node = esp_rmaker_node_init(&rmaker_config, "Test Node", "generic");
    TEST_ASSERT_NOT_NULL(test_node);

    // Add an attribute to the node
    esp_err_t err = esp_rmaker_node_add_attribute(test_node, "room", "living_room");
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Add firmware version
    err = esp_rmaker_node_add_fw_version(test_node, "1.0.0");
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Add model
    err = esp_rmaker_node_add_model(test_node, "esp32-c3-devkit");
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Add subtype
    err = esp_rmaker_node_add_subtype(test_node, "Development Kit");
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("ESP RainMaker Device Creation", "[esp_rmaker_core]")
{
    // Verify node exists
    TEST_ASSERT_NOT_NULL(test_node);

    // Create a light device
    test_device = esp_rmaker_lightbulb_device_create("Light", NULL, NULL);
    TEST_ASSERT_NOT_NULL(test_device);

    // Add a callback for handling write/read requests
    esp_err_t err = esp_rmaker_device_add_cb(test_device, write_cb, read_cb);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Add device to node
    err = esp_rmaker_node_add_device(test_node, test_device);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Add some attributes to the device
    err = esp_rmaker_device_add_attribute(test_device, "color", "rgb");
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_device_add_attribute(test_device, "location", "ceiling");
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("ESP RainMaker Core NULL and invalid input", "[esp_rmaker_core]")
{
    TEST_ASSERT_NOT_NULL(test_node);
    TEST_ASSERT_NOT_NULL(test_device);

    esp_err_t err;
    esp_rmaker_param_t *param;

    err = esp_rmaker_node_add_device(NULL, test_device);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = esp_rmaker_node_add_device(test_node, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = esp_rmaker_node_add_attribute(NULL, "room", "x");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = esp_rmaker_node_add_attribute(test_node, NULL, "x");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = esp_rmaker_node_add_attribute(test_node, "room", NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = esp_rmaker_node_add_fw_version(NULL, "1.0.0");
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = esp_rmaker_node_add_fw_version(test_node, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    param = esp_rmaker_param_create(NULL, ESP_RMAKER_PARAM_POWER,
                                   esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NULL(param);
    err = esp_rmaker_node_remove_device(NULL, test_device);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = esp_rmaker_node_remove_device(test_node, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
}

TEST_CASE("ESP RainMaker Parameter Operations", "[esp_rmaker_core]")
{
    // Verify device exists
    TEST_ASSERT_NOT_NULL(test_device);
    test_param = esp_rmaker_param_create("power", ESP_RMAKER_PARAM_POWER,
                                        esp_rmaker_bool(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(test_param);

    // Add parameter to device
    esp_err_t err = esp_rmaker_device_add_param(test_device, test_param);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Set as primary parameter
    err = esp_rmaker_device_assign_primary_param(test_device, test_param);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Create a brightness parameter
    esp_rmaker_param_t *brightness_param = esp_rmaker_param_create("brightness",
                                            ESP_RMAKER_PARAM_BRIGHTNESS,
                                            esp_rmaker_int(50),
                                            PROP_FLAG_READ | PROP_FLAG_WRITE);
    TEST_ASSERT_NOT_NULL(brightness_param);

    // Add bounds to the brightness parameter
    err = esp_rmaker_param_add_bounds(brightness_param,
                                    esp_rmaker_int(0),
                                    esp_rmaker_int(100),
                                    esp_rmaker_int(1));
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Add parameter to device
    err = esp_rmaker_device_add_param(test_device, brightness_param);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("ESP RainMaker Parameter Update", "[esp_rmaker_core]")
{
    TEST_ASSERT_NOT_NULL(test_param);
    write_cb_semaphore = xSemaphoreCreateBinary();
    TEST_ASSERT_NOT_NULL(write_cb_semaphore);
    test_param_val = esp_rmaker_bool(true);
    param_update_called = false;

    esp_err_t err = esp_rmaker_param_update(test_param, test_param_val);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Report the parameter value
    err = esp_rmaker_param_update_and_report(test_param, test_param_val);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // This won't trigger our callback as we're just updating the value locally
    TEST_ASSERT_EQUAL(false, param_update_called);

    vSemaphoreDelete(write_cb_semaphore);
}

TEST_CASE("ESP RainMaker Device Removal", "[esp_rmaker_core]")
{
    // Verify device exists
    TEST_ASSERT_NOT_NULL(test_device);
    TEST_ASSERT_NOT_NULL(test_node);

    // Remove device from node
    esp_err_t err = esp_rmaker_node_remove_device(test_node, test_device);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    // Delete device
    err = esp_rmaker_device_delete(test_device);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    test_device = NULL;
    test_param = NULL; // Parameters are deleted with the device
}
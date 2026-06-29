/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_rmaker_core.h"
#include "esp_rmaker_auth_service.h"
#include "esp_rmaker_standard_types.h"
#include "esp_rmaker_standard_params.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define AUTH_SERVICE_DEVICE_NAME  "RMUserAuth"

static void auth_service_test_enable(void)
{
    esp_err_t err = esp_rmaker_auth_service_enable();
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

static void auth_service_test_disable_and_cleanup(void)
{
    esp_rmaker_auth_service_disable();
}

TEST_CASE("Auth service token status update", "[esp_rmaker_auth_service]")
{
    esp_err_t err = esp_rmaker_user_auth_service_token_status_update(
        ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_VERIFIED);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    auth_service_test_enable();
    err = esp_rmaker_user_auth_service_token_status_update(
        ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_MAX);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);
    err = esp_rmaker_user_auth_service_token_status_update(
        ESP_RMAKER_USER_AUTH_SERVICE_TOKEN_STATUS_VERIFIED);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    auth_service_test_disable_and_cleanup();
}

TEST_CASE("Auth service get token and URL when disabled or NULL", "[esp_rmaker_auth_service]")
{
    esp_err_t err;
    char *out = NULL;

    err = esp_rmaker_auth_service_get_user_token(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    err = esp_rmaker_auth_service_get_base_url(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);

    err = esp_rmaker_auth_service_get_user_token(&out);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    TEST_ASSERT_NULL(out);
    out = NULL;
    err = esp_rmaker_auth_service_get_base_url(&out);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    TEST_ASSERT_NULL(out);
}

TEST_CASE("Auth service enable and disable lifecycle", "[esp_rmaker_auth_service]")
{
    esp_err_t err;
    char *out = NULL;

    err = esp_rmaker_auth_service_enable();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_auth_service_enable();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_auth_service_disable();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_auth_service_disable();
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* Verify service is actually disabled: get_user_token returns NOT_FOUND */
    err = esp_rmaker_auth_service_get_user_token(&out);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, err);
    TEST_ASSERT_NULL(out);
}

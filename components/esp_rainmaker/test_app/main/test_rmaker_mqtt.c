/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "esp_rmaker_mqtt.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"

static const char *TAG = "rmaker_mqtt_test";
static bool mqtt_connected = false;
static bool publish_cb_called = false;
static bool mock_subscribe_called = false;
static bool mock_unsubscribe_called = false;

// MQTT connect callback
static void mqtt_connect_cb(void *priv_data)
{
    ESP_LOGI(TAG, "MQTT Connected");
    mqtt_connected = true;
}

// MQTT disconnect callback
static void mqtt_disconnect_cb(void *priv_data)
{
    ESP_LOGI(TAG, "MQTT Disconnected");
    mqtt_connected = false;
}

// MQTT publish callback
static void mqtt_publish_cb(void *priv_data)
{
    ESP_LOGI(TAG, "Data published successfully");
    publish_cb_called = true;
}

// MQTT data callback — subscribe handler reused by the subscribe tests.
static void mqtt_data_cb(const char *topic, void *data, size_t data_len, void *priv_data)
{
    ESP_LOGI(TAG, "Data received on topic: %s, len: %d", topic, data_len);
}

/* In-test mock: implements esp_rmaker_mqtt_config_t and invokes connect/disconnect/publish callbacks. */
static esp_err_t mock_mqtt_init(esp_rmaker_mqtt_conn_params_t *conn_params)
{
    (void)conn_params;
    mqtt_connect_cb(NULL);
    return ESP_OK;
}

static void mock_mqtt_deinit(void)
{
    mqtt_disconnect_cb(NULL);
}

static esp_err_t mock_mqtt_connect(void)
{
    return ESP_OK;
}

static esp_err_t mock_mqtt_disconnect(void)
{
    return ESP_OK;
}

static esp_err_t mock_mqtt_publish(const char *topic, void *data, size_t data_len, uint8_t qos, int *msg_id)
{
    (void)topic;
    (void)data;
    (void)data_len;
    (void)qos;
    if (msg_id) {
        *msg_id = 1;
    }
    mqtt_publish_cb(NULL);
    return ESP_OK;
}

static esp_err_t mock_mqtt_subscribe(const char *topic, esp_rmaker_mqtt_subscribe_cb_t cb, uint8_t qos, void *priv_data)
{
    (void)topic;
    (void)cb;
    (void)qos;
    (void)priv_data;
    mock_subscribe_called = true;
    return ESP_OK;
}

static esp_err_t mock_mqtt_unsubscribe(const char *topic)
{
    (void)topic;
    mock_unsubscribe_called = true;
    return ESP_OK;
}

static void install_mock_mqtt(void)
{
    esp_rmaker_mqtt_config_t mock_config = {
        .setup_done = true,
        .get_conn_params = NULL,
        .init = mock_mqtt_init,
        .deinit = mock_mqtt_deinit,
        .connect = mock_mqtt_connect,
        .disconnect = mock_mqtt_disconnect,
        .publish = mock_mqtt_publish,
        .subscribe = mock_mqtt_subscribe,
        .unsubscribe = mock_mqtt_unsubscribe,
    };
    esp_rmaker_mqtt_setup(mock_config);
}

TEST_CASE("ESP RainMaker MQTT Init", "[rmaker_mqtt]")
{
    esp_netif_init();
    mqtt_connected = false;
    esp_rmaker_mqtt_conn_params_t conn_params = { 0 };
    esp_err_t err = esp_rmaker_mqtt_init(&conn_params);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    /* Verify API is usable after init: subscribe path works (glue or config is registered) */
    err = esp_rmaker_mqtt_subscribe("test/init", mqtt_data_cb, 0, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);
}

TEST_CASE("ESP RainMaker MQTT NULL and invalid input", "[rmaker_mqtt]")
{
    esp_netif_init();
    const char *topic = "test/topic";
    const char *data = "data";
    int msg_id;

    esp_err_t err = esp_rmaker_mqtt_subscribe(NULL, mqtt_data_cb, 0, NULL);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);
    err = esp_rmaker_mqtt_subscribe(topic, NULL, 0, NULL);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);
    err = esp_rmaker_mqtt_unsubscribe(NULL);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);
    /* publish with NULL topic or NULL data: API delegates to glue, which returns ESP_FAIL */
    err = esp_rmaker_mqtt_publish(NULL, (void *)data, strlen(data), 0, &msg_id);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);
    err = esp_rmaker_mqtt_publish(topic, NULL, strlen(data), 0, &msg_id);
    TEST_ASSERT_EQUAL(ESP_FAIL, err);
}

TEST_CASE("ESP RainMaker MQTT with mock: init invokes connect callback", "[rmaker_mqtt]")
{
    esp_netif_init();
    mqtt_connected = false;
    install_mock_mqtt();
    esp_rmaker_mqtt_conn_params_t conn_params = { 0 };
    esp_err_t err = esp_rmaker_mqtt_init(&conn_params);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(mqtt_connected);
    esp_rmaker_mqtt_deinit();
}

TEST_CASE("ESP RainMaker MQTT with mock: publish invokes publish callback", "[rmaker_mqtt]")
{
    esp_netif_init();
    install_mock_mqtt();
    esp_rmaker_mqtt_conn_params_t conn_params = { 0 };
    esp_err_t err = esp_rmaker_mqtt_init(&conn_params);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    publish_cb_called = false;
    const char *topic = "test/publish";
    const char *data = "{\"test\":\"data\"}";
    int msg_id = 0;
    err = esp_rmaker_mqtt_publish(topic, (void *)data, strlen(data), 0, &msg_id);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(publish_cb_called);
    esp_rmaker_mqtt_deinit();
}

TEST_CASE("ESP RainMaker MQTT with mock: deinit invokes disconnect callback", "[rmaker_mqtt]")
{
    esp_netif_init();
    mqtt_connected = false;
    install_mock_mqtt();
    esp_rmaker_mqtt_conn_params_t conn_params = { 0 };
    esp_err_t err = esp_rmaker_mqtt_init(&conn_params);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(mqtt_connected);
    esp_rmaker_mqtt_deinit();
    TEST_ASSERT_FALSE(mqtt_connected);
}

TEST_CASE("ESP RainMaker MQTT Subscribe", "[rmaker_mqtt]")
{
    esp_netif_init();
    install_mock_mqtt();
    esp_rmaker_mqtt_conn_params_t conn_params = { 0 };
    esp_err_t err = esp_rmaker_mqtt_init(&conn_params);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    mock_subscribe_called = false;
    const char *test_topic = "test/topic";
    err = esp_rmaker_mqtt_subscribe(test_topic, mqtt_data_cb, 0, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(mock_subscribe_called);

    mock_subscribe_called = false;
    const char *test_topic2 = "test/topic2";
    err = esp_rmaker_mqtt_subscribe(test_topic2, mqtt_data_cb, 1, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(mock_subscribe_called);

    esp_rmaker_mqtt_deinit();
}

TEST_CASE("ESP RainMaker MQTT Unsubscribe", "[rmaker_mqtt]")
{
    esp_netif_init();
    install_mock_mqtt();
    esp_rmaker_mqtt_conn_params_t conn_params = { 0 };
    esp_err_t err = esp_rmaker_mqtt_init(&conn_params);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_mqtt_subscribe("test/topic", mqtt_data_cb, 0, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    mock_unsubscribe_called = false;
    err = esp_rmaker_mqtt_unsubscribe("test/topic");
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_TRUE(mock_unsubscribe_called);

    esp_rmaker_mqtt_deinit();
}

TEST_CASE("ESP RainMaker MQTT Publish", "[rmaker_mqtt]")
{
    esp_netif_init();
    install_mock_mqtt();
    esp_rmaker_mqtt_conn_params_t conn_params = { 0 };
    esp_err_t err = esp_rmaker_mqtt_init(&conn_params);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    const char *test_topic = "test/publish";
    const char *test_data = "{\"test\":\"data\"}";
    int msg_id;

    err = esp_rmaker_mqtt_publish(test_topic, (void *)test_data, strlen(test_data), 0, &msg_id);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_mqtt_publish(test_topic, (void *)test_data, strlen(test_data), 1, &msg_id);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    err = esp_rmaker_mqtt_publish(test_topic, (void *)test_data, strlen(test_data), 0, &msg_id);
    TEST_ASSERT_EQUAL(ESP_OK, err);

    TEST_ASSERT_TRUE(publish_cb_called);

    esp_rmaker_mqtt_deinit();
}

TEST_CASE("ESP RainMaker MQTT Cleanup", "[rmaker_mqtt]")
{
    esp_netif_init();
    esp_rmaker_mqtt_deinit();
}

/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <inttypes.h>
#include <string.h>
#include "esp_log.h"
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_params.h>

#include "rmaker_camera.h"
#include "credentials_provider.h"
#include "app_webrtc.h"
#include "kvs_signaling.h"

static const char *TAG = "rmaker_camera";

/* Create a RainMaker camera device with name and channel parameters */
esp_rmaker_device_t *esp_rmaker_camera_device_create(const char *dev_name, void *priv_data)
{
    esp_rmaker_device_t *device = esp_rmaker_device_create(dev_name, ESP_RMAKER_DEVICE_CAMERA, priv_data);
    if (device) {
        /* Add name parameter */
        esp_rmaker_device_add_param(device, esp_rmaker_name_param_create(ESP_RMAKER_DEF_NAME_PARAM, dev_name));

        /* Create and add channel parameter using the node ID */
        const char *node_id = esp_rmaker_get_node_id();
        if (node_id) {
            char channel_name[32] = {0};
            snprintf(channel_name, sizeof(channel_name), "%s%s", ESP_RMAKER_CHANNEL_NAME_PREFIX, node_id);

            esp_rmaker_param_t *channel_param = esp_rmaker_param_create(
                ESP_RMAKER_DEF_CHANNEL_NAME, ESP_RMAKER_PARAM_CHANNEL, esp_rmaker_str(channel_name), PROP_FLAG_READ);
            if (channel_param) {
                esp_rmaker_device_add_param(device, channel_param);
            }
        }
    }
    return device;
}

/* Event handler for WebRTC events */
static void app_webrtc_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    if (event_data == NULL) {
        return;
    }

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_INITIALIZED:
            ESP_LOGI(TAG, "[KVS Event] WebRTC Initialized.");
            break;
        case APP_WEBRTC_EVENT_DEINITIALIZING:
            ESP_LOGI(TAG, "[KVS Event] WebRTC Deinitialized.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTING:
            ESP_LOGI(TAG, "[KVS Event] Signaling Connecting.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_CONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Signaling Connected.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Signaling Disconnected.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_DESCRIBE:
            ESP_LOGI(TAG, "[KVS Event] Signaling Describe.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_GET_ENDPOINT:
            ESP_LOGI(TAG, "[KVS Event] Signaling Get Endpoint.");
            break;
        case APP_WEBRTC_EVENT_SIGNALING_GET_ICE:
            ESP_LOGI(TAG, "[KVS Event] Signaling Get ICE.");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTION_REQUESTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Connection Requested.");
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Connected: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_PEER_DISCONNECTED:
            ESP_LOGI(TAG, "[KVS Event] Peer Disconnected: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_STREAMING_STARTED:
            ESP_LOGI(TAG, "[KVS Event] Streaming Started for Peer: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_STREAMING_STOPPED:
            ESP_LOGI(TAG, "[KVS Event] Streaming Stopped for Peer: %s", event_data->peer_id);
            break;
        case APP_WEBRTC_EVENT_RECEIVED_OFFER:
            ESP_LOGI(TAG, "[KVS Event] Received Offer.");
            break;
        case APP_WEBRTC_EVENT_SENT_ANSWER:
            ESP_LOGI(TAG, "[KVS Event] Sent Answer.");
            break;
        case APP_WEBRTC_EVENT_ERROR:
        case APP_WEBRTC_EVENT_SIGNALING_ERROR:
        case APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED:
            ESP_LOGE(TAG, "[KVS Event] Error Event %d: Code %d, Message: %s",
                     (int)event_data->event_id, (int)event_data->status_code, event_data->message);
            break;
        default:
            ESP_LOGI(TAG, "[KVS Event] Unhandled Event ID: %d", (int) event_data->event_id);
            break;
    }
}

void rmaker_webrtc_camera_init(rmaker_webrtc_camera_config_t *config)
{
    if (config == NULL) {
        ESP_LOGE(TAG, "Config parameter cannot be NULL");
        return;
    }

    if (config->peer_connection_if == NULL) {
        ESP_LOGE(TAG, "peer_connection_if cannot be NULL");
        return;
    }

    /* Register the event callback */
    if (app_webrtc_register_event_callback(app_webrtc_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register KVS event callback.");
    }

    /* Call mode-specific initialization callback if provided */
    if (config->init_callback) {
        if (config->init_callback(config->init_callback_user_data) != 0) {
            ESP_LOGE(TAG, "Mode-specific initialization callback failed");
            return;
        }
    }

    /* Create channel name using the node ID */
    const char *node_id = esp_rmaker_get_node_id();
    static char channel_name[32] = {0};
    snprintf(channel_name, sizeof(channel_name), "%s%s", ESP_RMAKER_CHANNEL_NAME_PREFIX, node_id);

    /* Get AWS region using new RainMaker core function (Never freed) */
    char *aws_region = esp_rmaker_get_aws_region();
    if (!aws_region) {
        ESP_LOGE(TAG, "Failed to get AWS region");
        return;
    }

    /* Set up KVS signaling configuration (all KVS-specific details) */
    static kvs_signaling_config_t kvs_signaling_cfg = {0};

    /* Channel configuration */
    kvs_signaling_cfg.pChannelName = channel_name;

    /* Prefer callback-based credentials supplier; the callback will handle fetching tokens */
    kvs_signaling_cfg.useIotCredentials = false;
    kvs_signaling_cfg.iotCoreCredentialEndpoint = NULL;
    kvs_signaling_cfg.iotCoreCert = NULL;
    kvs_signaling_cfg.iotCorePrivateKey = NULL;
    kvs_signaling_cfg.iotCoreRoleAlias = NULL;
    kvs_signaling_cfg.iotCoreThingName = NULL;

    /* Common AWS options */
    kvs_signaling_cfg.awsRegion = aws_region;  /* Region is needed for STUN URL */
    kvs_signaling_cfg.fetch_credentials_cb = rmaker_fetch_aws_credentials;
    kvs_signaling_cfg.fetch_credentials_user_data = 0;

    /* Configure WebRTC app with our new simplified API */
    app_webrtc_config_t app_webrtc_config = APP_WEBRTC_CONFIG_DEFAULT();

    /* Signaling configuration (passed as opaque pointers) */
    app_webrtc_config.signaling_client_if = kvs_signaling_client_if_get();
    app_webrtc_config.signaling_cfg = &kvs_signaling_cfg;

    /* Use peer connection and media interfaces from config */
    app_webrtc_config.peer_connection_if = (webrtc_peer_connection_if_t *)config->peer_connection_if;
    app_webrtc_config.video_capture = config->video_capture;
    app_webrtc_config.audio_capture = config->audio_capture;
    app_webrtc_config.video_player = config->video_player;
    app_webrtc_config.audio_player = config->audio_player;

    /* Initialize WebRTC application with simplified API and reasonable defaults */
    ESP_LOGI(TAG, "Initializing WebRTC application with simplified configuration:");
    ESP_LOGI(TAG, "  - Role: MASTER (Rainmaker cameras initiate connections)");
    ESP_LOGI(TAG, "  - Media type: auto-detected (audio+video from interfaces)");
    ESP_LOGI(TAG, "  - Codecs: OPUS/H264 (optimal for streaming)");
    ESP_LOGI(TAG, "  - ICE: trickle enabled, TURN enabled (best connectivity)");
    ESP_LOGI(TAG, "  - Channel: %s", kvs_signaling_cfg.pChannelName);
    ESP_LOGI(TAG, "  - Region: %s", aws_region);
    if (kvs_signaling_cfg.fetch_credentials_cb) {
        ESP_LOGI(TAG, "  - Credentials: callback-based (RainMaker core function)");
    } else {
        ESP_LOGI(TAG, "  - Endpoint: %s", kvs_signaling_cfg.iotCoreCredentialEndpoint);
    }

    WEBRTC_STATUS status = app_webrtc_init(&app_webrtc_config);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize WebRTC application: 0x%08" PRIx32, (uint32_t)status);
        return;
    }

    /* Run WebRTC application (this will create a task internally) */
    ESP_LOGI(TAG, "Starting WebRTC application");
    status = app_webrtc_run();
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start WebRTC application: 0x%08" PRIx32, (uint32_t)status);
        app_webrtc_terminate();
    } else {
        ESP_LOGI(TAG, "WebRTC application started successfully");
    }
}

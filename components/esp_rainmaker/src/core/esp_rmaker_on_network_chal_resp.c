/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <protocomm.h>
#include <protocomm_httpd.h>
#include <protocomm_security0.h>
#include <protocomm_security1.h>
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
#include <mdns.h>
#endif

#include <esp_rmaker_core.h>
#include <esp_rmaker_on_network_chal_resp.h>
#include "esp_rmaker_internal.h"

#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2
#include <protocomm_security2.h>
#endif

/* Delay before stopping service after disable (in microseconds).
 * 2 seconds allows time for the CLI to receive the disable response
 * before the HTTP server is shut down. */
#define STOP_DELAY_US   (2 * 1000 * 1000)  /* 2 seconds */

static const char *TAG = "on_network_chal_resp";

#define MDNS_SERVICE_TYPE       "_esp_rmaker_chal_resp"
#define MDNS_SERVICE_PROTO      "_tcp"
/* Use esp_local_ctrl endpoints for consistency with Local Control.
 * This allows CLI to use the same logic for both on-network and local control. */
#define SECURITY_ENDPOINT       "esp_local_ctrl/session"
#define CHAL_RESP_ENDPOINT      "ch_resp"
#define VERSION_ENDPOINT        "esp_local_ctrl/version"
#define VERSION_STRING          "{\"on_network\":{\"ver\":\"v1.0\",\"cap\":[\"ch_resp\"]}}"

/* Helper macros for mDNS TXT record operations */
#define MDNS_TXT_SET(key, value) \
    mdns_service_txt_item_set(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, key, value)

#define MDNS_TXT_REMOVE(key) \
    mdns_service_txt_item_remove(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, key)

/* State structure for on-network challenge-response service */
typedef struct {
    protocomm_t *pc;                            /* Protocomm instance */
    bool running;                               /* Service running state */
    bool cleanup_done;                          /* mDNS cleanup done flag */
    bool stop_scheduled;                        /* Stop timer scheduled flag */
    uint16_t port;                              /* HTTP server port */
    esp_rmaker_on_network_sec_t sec_ver;        /* Security version */
    bool mdns_enabled;                          /* mDNS enabled flag */
    char *mdns_instance_name;                   /* mDNS instance name */
    protocomm_security1_params_t *sec1_params;  /* Security 1 params (if used) */
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2
    protocomm_security2_params_t *sec2_params;  /* Security 2 params (if used) */
#endif
    esp_timer_handle_t stop_timer;              /* Timer for delayed stop */
} on_network_chal_resp_state_t;

static on_network_chal_resp_state_t s_state = {0};

/* Timer callback for delayed stop */
static void on_network_stop_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Stop timer fired, stopping on-network challenge-response service");
    esp_rmaker_on_network_chal_resp_stop();
}

/* Forward declaration */
static esp_err_t on_network_chal_resp_do_cleanup(void);

/* Schedule delayed stop of the service */
static esp_err_t schedule_delayed_stop(void)
{
    if (s_state.stop_scheduled) {
        ESP_LOGD(TAG, "Stop already scheduled");
        return ESP_OK;
    }

    if (!s_state.stop_timer) {
        esp_timer_create_args_t timer_args = {
            .callback = on_network_stop_timer_cb,
            .arg = NULL,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "on_net_cr_stop"
        };
        esp_err_t err = esp_timer_create(&timer_args, &s_state.stop_timer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create stop timer: %s", esp_err_to_name(err));
            return err;
        }
    }

    esp_err_t err = esp_timer_start_once(s_state.stop_timer, STOP_DELAY_US);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start stop timer: %s", esp_err_to_name(err));
        esp_timer_delete(s_state.stop_timer);
        s_state.stop_timer = NULL;
        return err;
    }

    s_state.stop_scheduled = true;
    ESP_LOGI(TAG, "Service stop scheduled in %d ms", STOP_DELAY_US / 1000);
    return ESP_OK;
}

/* Wrapper handler for on-network challenge-response */
static esp_err_t on_network_chal_resp_wrapper(uint32_t session_id, const uint8_t *inbuf,
                                               ssize_t inlen, uint8_t **outbuf,
                                               ssize_t *outlen, void *priv_data)
{
    bool was_enabled = !esp_rmaker_chal_resp_is_disabled();

    /* Call the core handler */
    esp_err_t ret = esp_rmaker_chal_resp_handler(session_id, inbuf, inlen,
                                                  outbuf, outlen, priv_data);

    /* If chal_resp was just disabled via the endpoint, do on-network cleanup and schedule stop */
    if (was_enabled && esp_rmaker_chal_resp_is_disabled()) {
        esp_rmaker_on_network_chal_resp_disable();
        schedule_delayed_stop();
    }

    return ret;
}

#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
static esp_err_t announce_mdns_service(const esp_rmaker_on_network_chal_resp_config_t *config)
{
    esp_err_t err;

    /* Initialize mDNS if not already initialized */
    err = mdns_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        /* ESP_ERR_INVALID_STATE means already initialized, which is fine */
        ESP_LOGE(TAG, "Failed to initialize mDNS: %s", esp_err_to_name(err));
        return err;
    }

    /* Set hostname to node_id */
    const char *node_id = esp_rmaker_get_node_id();
    if (node_id) {
        mdns_hostname_set(node_id);
    }

    /* Add the service */
    err = mdns_service_add(NULL, MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, s_state.port, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add mDNS service: %s", esp_err_to_name(err));
        return err;
    }

    /* Set instance name */
    const char *instance_name = config->mdns_instance_name;
    if (!instance_name && node_id) {
        instance_name = node_id;
    }
    if (instance_name) {
        mdns_service_instance_name_set(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, instance_name);
        s_state.mdns_instance_name = strdup(instance_name);
        if (!s_state.mdns_instance_name) {
            ESP_LOGE(TAG, "Failed to allocate memory for instance name");
            mdns_service_remove(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO);
            return ESP_ERR_NO_MEM;
        }
    }

    /* Add TXT records */
    if (node_id) {
        MDNS_TXT_SET("node_id", node_id);
    }

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", s_state.port);
    MDNS_TXT_SET("port", port_str);

    char sec_ver_str[2];
    snprintf(sec_ver_str, sizeof(sec_ver_str), "%d", config->sec_ver);
    MDNS_TXT_SET("sec_version", sec_ver_str);

    if (config->sec_ver == ESP_RMAKER_ON_NETWORK_SEC1) {
        MDNS_TXT_SET("pop_required", config->pop ? "true" : "false");
    }

    ESP_LOGI(TAG, "mDNS service announced: %s.%s, port: %d", MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO, s_state.port);
    return ESP_OK;
}

static void remove_mdns_service(void)
{
    mdns_service_remove(MDNS_SERVICE_TYPE, MDNS_SERVICE_PROTO);
    if (s_state.mdns_instance_name) {
        free(s_state.mdns_instance_name);
        s_state.mdns_instance_name = NULL;
    }
}
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */

static esp_err_t setup_security(protocomm_t *pc, const esp_rmaker_on_network_chal_resp_config_t *config)
{
    esp_err_t err = ESP_OK;

    switch (config->sec_ver) {
        case ESP_RMAKER_ON_NETWORK_SEC_NONE:
            ESP_LOGW(TAG, "Using Security 0 (no security) - NOT recommended for production");
            err = protocomm_set_security(pc, SECURITY_ENDPOINT, &protocomm_security0, NULL);
            break;

        case ESP_RMAKER_ON_NETWORK_SEC1:
            ESP_LOGI(TAG, "Using Security 1 (Curve25519 + AES-CTR)");
            if (config->pop && strlen(config->pop) > 0) {
                s_state.sec1_params = calloc(1, sizeof(protocomm_security1_params_t));
                if (!s_state.sec1_params) {
                    ESP_LOGE(TAG, "Failed to allocate memory for security params");
                    return ESP_ERR_NO_MEM;
                }
                s_state.sec1_params->data = (const uint8_t *)config->pop;
                s_state.sec1_params->len = strlen(config->pop);
                err = protocomm_set_security(pc, SECURITY_ENDPOINT, &protocomm_security1, s_state.sec1_params);
                ESP_LOGI(TAG, "Security 1 configured with POP");
            } else {
                err = protocomm_set_security(pc, SECURITY_ENDPOINT, &protocomm_security1, NULL);
                ESP_LOGI(TAG, "Security 1 configured without POP");
            }
            break;

        case ESP_RMAKER_ON_NETWORK_SEC2:
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2
            ESP_LOGI(TAG, "Using Security 2 (SRP6a + AES-GCM)");
            if (!config->salt || !config->verifier || config->salt_len == 0 || config->verifier_len == 0) {
                ESP_LOGE(TAG, "Security 2 requires salt and verifier");
                return ESP_ERR_INVALID_ARG;
            }
            s_state.sec2_params = calloc(1, sizeof(protocomm_security2_params_t));
            if (!s_state.sec2_params) {
                ESP_LOGE(TAG, "Failed to allocate memory for security params");
                return ESP_ERR_NO_MEM;
            }
            s_state.sec2_params->salt = (const char *)config->salt;
            s_state.sec2_params->salt_len = config->salt_len;
            s_state.sec2_params->verifier = (const char *)config->verifier;
            s_state.sec2_params->verifier_len = config->verifier_len;
            err = protocomm_set_security(pc, SECURITY_ENDPOINT, &protocomm_security2, s_state.sec2_params);
#else
            ESP_LOGE(TAG, "Security 2 not supported. Enable CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2");
            return ESP_ERR_NOT_SUPPORTED;
#endif
            break;

        default:
            ESP_LOGE(TAG, "Invalid security version: %d", config->sec_ver);
            return ESP_ERR_INVALID_ARG;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set security: %s", esp_err_to_name(err));
    }
    return err;
}

static void cleanup_security_params(void)
{
    if (s_state.sec1_params) {
        free(s_state.sec1_params);
        s_state.sec1_params = NULL;
    }
#ifdef CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_2
    if (s_state.sec2_params) {
        free(s_state.sec2_params);
        s_state.sec2_params = NULL;
    }
#endif
}

esp_err_t esp_rmaker_on_network_chal_resp_start(const esp_rmaker_on_network_chal_resp_config_t *config)
{
    if (s_state.running) {
        ESP_LOGE(TAG, "Service already running");
        return ESP_ERR_INVALID_STATE;
    }

    /* Enable the core challenge-response handler (in case it was disabled previously) */
    esp_rmaker_chal_resp_enable();

    /* Use default config if none provided */
    esp_rmaker_on_network_chal_resp_config_t default_config = ESP_RMAKER_ON_NETWORK_CHAL_RESP_DEFAULT_CONFIG();
    if (!config) {
        config = &default_config;
    }

    esp_err_t err;

    /* Create protocomm instance */
    s_state.pc = protocomm_new();
    if (!s_state.pc) {
        ESP_LOGE(TAG, "Failed to create protocomm instance");
        return ESP_ERR_NO_MEM;
    }

    /* Configure and start HTTP server */
    s_state.port = config->port ? config->port : 80;
    protocomm_httpd_config_t httpd_config = {
        .ext_handle_provided = false,
        .data = {
            .config = {
                .port = s_state.port,
                .stack_size = 4096,
                .task_priority = tskIDLE_PRIORITY + 5
            }
        }
    };

    err = protocomm_httpd_start(s_state.pc, &httpd_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        goto cleanup_pc;
    }
    ESP_LOGI(TAG, "HTTP server started on port %d", s_state.port);

    /* Setup security */
    err = setup_security(s_state.pc, config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup security");
        goto cleanup_httpd;
    }

    /* Set version endpoint */
    err = protocomm_set_version(s_state.pc, VERSION_ENDPOINT, VERSION_STRING);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set version endpoint: %s", esp_err_to_name(err));
        goto cleanup_security;
    }

    /* Register challenge-response endpoint (using wrapper for post-disable cleanup) */
    err = protocomm_add_endpoint(s_state.pc, CHAL_RESP_ENDPOINT, on_network_chal_resp_wrapper, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register challenge-response endpoint: %s", esp_err_to_name(err));
        goto cleanup_version;
    }
    ESP_LOGI(TAG, "Challenge-response endpoint registered: %s", CHAL_RESP_ENDPOINT);

    /* Announce mDNS service if enabled */
    s_state.sec_ver = config->sec_ver;
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
    s_state.mdns_enabled = config->enable_mdns;
    if (config->enable_mdns) {
        err = announce_mdns_service(config);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to announce mDNS service, but service will still be accessible via IP");
            /* Don't fail the entire start, mDNS is optional */
        }
    }
#else
    s_state.mdns_enabled = false;
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */

    s_state.running = true;
    ESP_LOGI(TAG, "On-network challenge-response service started successfully");
    ESP_LOGI(TAG, "  Port: %d", s_state.port);
    ESP_LOGI(TAG, "  Security: %d", s_state.sec_ver);
    ESP_LOGI(TAG, "  mDNS: %s", s_state.mdns_enabled ? "enabled" : "disabled");

    return ESP_OK;

cleanup_version:
    protocomm_unset_version(s_state.pc, VERSION_ENDPOINT);
cleanup_security:
    protocomm_unset_security(s_state.pc, SECURITY_ENDPOINT);
    cleanup_security_params();
cleanup_httpd:
    protocomm_httpd_stop(s_state.pc);
cleanup_pc:
    protocomm_delete(s_state.pc);
    s_state.pc = NULL;
    return err;
}

esp_err_t esp_rmaker_on_network_chal_resp_stop(void)
{
    if (!s_state.running) {
        ESP_LOGE(TAG, "Service not running");
        return ESP_ERR_INVALID_STATE;
    }

    /* Stop and delete the timer if it exists */
    if (s_state.stop_timer) {
        esp_timer_stop(s_state.stop_timer);
        esp_timer_delete(s_state.stop_timer);
        s_state.stop_timer = NULL;
    }

    /* Remove mDNS service */
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
    if (s_state.mdns_enabled) {
        remove_mdns_service();
    }
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */

    /* Remove endpoints */
    protocomm_remove_endpoint(s_state.pc, CHAL_RESP_ENDPOINT);
    protocomm_unset_version(s_state.pc, VERSION_ENDPOINT);
    protocomm_unset_security(s_state.pc, SECURITY_ENDPOINT);

    /* Cleanup security params */
    cleanup_security_params();

    /* Stop HTTP server and delete protocomm */
    protocomm_httpd_stop(s_state.pc);
    protocomm_delete(s_state.pc);

    /* Reset state */
    memset(&s_state, 0, sizeof(s_state));

    ESP_LOGI(TAG, "On-network challenge-response service stopped");
    return ESP_OK;
}

bool esp_rmaker_on_network_chal_resp_is_running(void)
{
    return s_state.running;
}

/* Internal function to re-announce mDNS service using stored state */
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
static esp_err_t reannounce_mdns_service(void)
{
    if (!s_state.mdns_enabled) {
        ESP_LOGD(TAG, "mDNS not enabled, skipping re-announce");
        return ESP_OK;
    }

    /* Remove existing service if it exists (in case enable is called without disable) */
    remove_mdns_service();

    /* Reconstruct config from stored state */
    esp_rmaker_on_network_chal_resp_config_t config = {
        .sec_ver = s_state.sec_ver,
        .port = s_state.port,
        .mdns_instance_name = s_state.mdns_instance_name,
        .enable_mdns = true
    };

    /* Set pop if Security 1 is used */
    if (s_state.sec_ver == ESP_RMAKER_ON_NETWORK_SEC1 && s_state.sec1_params) {
        config.pop = (const char *)s_state.sec1_params->data;
    }

    return announce_mdns_service(&config);
}
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */

/* Internal function to do on-network specific cleanup */
static esp_err_t on_network_chal_resp_do_cleanup(void)
{
    if (s_state.cleanup_done) {
        ESP_LOGD(TAG, "On-network chal_resp cleanup already done");
        return ESP_OK;
    }

    /* Remove mDNS service on disabling */
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
    if (s_state.mdns_enabled) {
        remove_mdns_service();
        ESP_LOGI(TAG, "mDNS service removed");
    }
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */

    s_state.cleanup_done = true;
    return ESP_OK;
}

esp_err_t esp_rmaker_on_network_chal_resp_disable(void)
{
    if (!s_state.running) {
        ESP_LOGW(TAG, "On-network challenge-response service not running");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Disabling challenge-response for on-network service");

    /* Call generic disable (idempotent) */
    esp_rmaker_chal_resp_disable();

    /* Do on-network specific cleanup (only once) */
    return on_network_chal_resp_do_cleanup();
}

esp_err_t esp_rmaker_on_network_chal_resp_enable(void)
{
    if (!s_state.running) {
        ESP_LOGW(TAG, "On-network challenge-response service not running");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Re-enabling challenge-response for on-network service");

    /* Enable the core challenge-response handler */
    esp_rmaker_chal_resp_enable();

    /* Reset cleanup flag */
    s_state.cleanup_done = false;

    /* Re-announce mDNS service */
#ifdef CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI
    if (s_state.mdns_enabled) {
        esp_err_t err = reannounce_mdns_service();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to re-announce mDNS service: %s", esp_err_to_name(err));
            /* Don't fail the entire enable, mDNS is optional */
        }
    }
#endif /* CONFIG_ESP_RMAKER_NETWORK_OVER_WIFI */

    return ESP_OK;
}


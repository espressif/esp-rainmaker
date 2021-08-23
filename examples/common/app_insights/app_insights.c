/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <esp_err.h>
#include <esp_log.h>
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#include <esp_rmaker_mqtt.h>
#include <esp_insights.h>
#include <string.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_factory.h>

#if CONFIG_APP_INSIGHTS_ENABLE_LOG_TYPE_ALL
#define APP_INSIGHTS_LOG_TYPE               ESP_DIAG_LOG_TYPE_ERROR \
                                            | ESP_DIAG_LOG_TYPE_WARNING \
                                            | ESP_DIAG_LOG_TYPE_EVENT
#else
#define APP_INSIGHTS_LOG_TYPE               ESP_DIAG_LOG_TYPE_ERROR
#endif /* CONFIG_APP_INSIGHTS_ENABLE_LOG_TYPE_ALL */

esp_err_t esp_insights_enable(esp_insights_config_t *config);

#endif /* CONFIG_ESP_INSIGHTS_ENABLED */

#define TAG "app_insights"

esp_err_t app_insights_enable(void)
{
#ifdef CONFIG_ESP_INSIGHTS_ENABLED
#ifdef CONFIG_ESP_RMAKER_SELF_CLAIM
    ESP_LOGW(TAG, "Nodes with Self Claiming may not be accessible for Insights.");
    /* This is required so that the esp insights component will get correct
     * node id from NVS
     */
    char *node_id = esp_rmaker_get_node_id();
    if (node_id) {
        esp_rmaker_factory_set("node_id", node_id, strlen(node_id));
    }
#endif
    esp_rmaker_mqtt_config_t mqtt_config = {
        .init           = NULL,
        .connect        = NULL,
        .disconnect     = NULL,
        .publish        = esp_rmaker_mqtt_publish,
        .subscribe      = esp_rmaker_mqtt_subscribe,
        .unsubscribe    = esp_rmaker_mqtt_unsubscribe,
    };
    esp_insights_mqtt_setup(mqtt_config);

    esp_insights_config_t config = {
        .log_type = APP_INSIGHTS_LOG_TYPE,
    };
    esp_insights_enable(&config);
#else
    ESP_LOGI(TAG, "Enable CONFIG_ESP_INSIGHTS_ENABLED to get Insights.");
#endif /* ! CONFIG_ESP_INSIGHTS_ENABLED */
    return ESP_OK;
}

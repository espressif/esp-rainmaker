set(srcs "src/esp_rmaker_ota.c")

if(CONFIG_ESP_RMAKER_OTA_USING_PARAMS)
    list(APPEND srcs
        "src/esp_rmaker_ota_using_params.c")
endif()
if(CONFIG_ESP_RMAKER_OTA_USING_TOPICS)
    list(APPEND srcs
        "src/esp_rmaker_ota_using_topics.c")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS src
                       REQUIRES
                       PRIV_REQUIRES esp_rmaker_core esp_rmaker_standard_types esp_rmaker_mqtt json_parser json_generator esp_http_client app_update esp_https_ota)

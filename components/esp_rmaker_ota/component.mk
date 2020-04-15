COMPONENT_SRCDIRS := src
COMPONENT_ADD_INCLUDEDIRS := include

ifndef CONFIG_ESP_RMAKER_OTA_USING_PARAMS
    COMPONENT_OBJEXCLUDE += src/esp_rmaker_ota_using_params.o
endif

ifndef CONFIG_ESP_RMAKER_OTA_USING_TOPICS
    COMPONENT_OBJEXCLUDE += src/esp_rmaker_ota_using_topics.o
endif

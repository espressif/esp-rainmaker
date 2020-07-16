COMPONENT_SRCDIRS := src/core src/mqtt src/ota src/standard_types src/console
COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_PRIV_INCLUDEDIRS := src/core src/ota src/console

ifndef CONFIG_ESP_RMAKER_SELF_CLAIM
    COMPONENT_OBJEXCLUDE += src/core/esp_rmaker_claim.o
endif

COMPONENT_EMBED_TXTFILES := server_certs/mqtt_server.crt server_certs/claim_service_server.crt server_certs/ota_server.crt

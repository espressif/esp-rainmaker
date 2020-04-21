COMPONENT_SRCDIRS := src
COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_PRIV_INCLUDEDIRS := src

ifndef CONFIG_ESP_RMAKER_SELF_CLAIM
    COMPONENT_OBJEXCLUDE += src/esp_rmaker_claim.o
endif

COMPONENT_EMBED_TXTFILES := server_certs/mqtt_server.crt server_certs/claim_service_server.crt

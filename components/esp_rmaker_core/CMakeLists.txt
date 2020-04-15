set(srcs "src/esp_rmaker_core.c"
        "src/esp_rmaker_node_config.c"
        "src/esp_rmaker_param_ops.c"
        "src/esp_rmaker_client_data.c"
        "src/esp_rmaker_time_sync.c"
        "src/esp_rmaker_storage.c"
        "src/esp_rmaker_user_mapping.pb-c.c"
        "src/esp_rmaker_utils.c"
        "src/esp_rmaker_user_mapping.c")

if(CONFIG_ESP_RMAKER_SELF_CLAIM)
    list(APPEND srcs
        "src/esp_rmaker_claim.c")
endif()

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS src
                       REQUIRES
                       PRIV_REQUIRES protobuf-c json_parser json_generator wifi_provisioning nvs_flash esp_http_client esp_rmaker_mqtt esp_rmaker_standard_types app_update esp-tls)
target_add_binary_data(${COMPONENT_TARGET} "server_certs/mqtt_server.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "server_certs/claiming_server.crt" TEXT)

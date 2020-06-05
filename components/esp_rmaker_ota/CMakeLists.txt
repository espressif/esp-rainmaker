idf_component_register(SRCS "src/esp_rmaker_ota.c"
                        "src/esp_rmaker_ota_using_params.c"
                        "src/esp_rmaker_ota_using_topics.c"
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS src
                       REQUIRES
                       PRIV_REQUIRES esp_rmaker_core esp_rmaker_standard_types esp_rmaker_mqtt json_parser json_generator esp_http_client app_update esp_https_ota)

target_add_binary_data(${COMPONENT_TARGET} "server_certs/ota_server.crt" TEXT)

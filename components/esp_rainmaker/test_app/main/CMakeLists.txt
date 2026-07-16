set (srcs "test_app_main.c"
                        "test_esp_rmaker_core.c"
                        "test_rmaker_mqtt.c"
                        "test_esp_rmaker_auth_service.c"
                        "test_esp_rmaker_groups.c"
                        "test_esp_rmaker_scenes.c"
                        "test_esp_rmaker_ota.c"
                        "test_esp_rmaker_schedule.c"
                        "test_esp_rmaker_node_auth.c"
                        "test_esp_rmaker_core_teardown.c")
set (priv_req unity esp_timer esp_rainmaker esp_netif esp_wifi esp_event nvs_flash app_update mbedtls rmaker_common)

idf_component_register(SRCS
                        ${srcs}
                        INCLUDE_DIRS "."
                        PRIV_REQUIRES ${priv_req}
                        REQUIRES freertos
                        WHOLE_ARCHIVE)

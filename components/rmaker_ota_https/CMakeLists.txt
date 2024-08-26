set(priv_req rmaker_common esp_rainmaker esp_timer esp_netif esp_http_client esp-tls app_update mbedtls esp_schedule json_parser json_generator)

idf_component_register(SRC_DIRS "src"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       PRIV_REQUIRES ${priv_req}
                       )

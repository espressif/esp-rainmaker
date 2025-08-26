idf_component_register(SRCS "app_wifi_with_homekit.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES network_provisioning esp_hap_core esp_hap_platform nvs_flash)

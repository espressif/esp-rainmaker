idf_component_register(SRCS "app_wifi_with_homekit.c"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_provisioning qrcode esp_hap_core esp_hap_platform)

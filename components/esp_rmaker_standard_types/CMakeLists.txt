idf_component_register(SRCS "src/esp_rmaker_standard_params.c"
                            "src/esp_rmaker_standard_devices.c"
                            "src/esp_rmaker_standard_services.c"
                       INCLUDE_DIRS include
                       REQUIRES esp_rmaker_core)

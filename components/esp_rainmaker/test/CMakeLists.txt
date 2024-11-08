idf_component_register(SRCS "test_rmaker.c"
                       INCLUDE_DIRS "."
                       PRIV_INCLUDE_DIRS . ${CMAKE_CURRENT_BINARY_DIR}
                       PRIV_REQUIRES unity nvs_flash esp_rainmaker app_network app_insights test_utils espressif__network_provisioning)

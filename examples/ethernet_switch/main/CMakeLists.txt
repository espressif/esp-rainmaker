set(priv_reqs app_ethernet esp_netif nvs_flash)

if(CONFIG_EXAMPLE_ENABLE_WIFI)
    list(APPEND priv_reqs rmaker_app_network)
endif()

idf_component_register(SRCS ./app_driver.c ./app_main.c ./app_on_network_test.c
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES ${priv_reqs})

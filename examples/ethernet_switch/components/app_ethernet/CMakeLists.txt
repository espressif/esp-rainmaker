idf_component_register(SRCS "app_ethernet.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES esp_netif esp_eth driver)

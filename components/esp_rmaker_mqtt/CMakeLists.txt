idf_component_register(SRCS "src/esp_rmaker_mqtt.c"
                       INCLUDE_DIRS include
                       PRIV_INCLUDE_DIRS src
                       REQUIRES
                       PRIV_REQUIRES mqtt)

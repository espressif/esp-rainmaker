set(srcs
    "src/credentials_provider.c"
    "src/rmaker_camera.c"
    "src/sleep_command.c"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES "nvs_flash"
             "esp_wifi"
             "esp_timer"
             "esp_netif"
             "esp_event"
             "mbedtls"
             "lwip"
    )


idf_component_register(
    SRCS
        "app_main.c"
        "power_save_handler.c"
        # Vendored from esp_hosted 3.x: CP light sleep ships as example code,
        # not as part of the component.
        "cp_light_sleep_controller.c"
        "host_ps_integration.c"
    INCLUDE_DIRS "."
    REQUIRES "nvs_flash"
             "esp_wifi"
             "esp_timer"
             "esp_netif"
             "esp_event"
             "mbedtls"
             "lwip"
             "esp_pm"
             # esp_hosted links its SDIO slave transport against this without
             # declaring it; the top-level COMPONENTS trim would drop it.
             "esp_driver_sdio"
    )

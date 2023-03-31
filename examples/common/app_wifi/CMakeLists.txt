set(priv_req wifi_provisioning qrcode nvs_flash esp_event rmaker_common)

idf_component_register(SRCS "app_wifi.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                    PRIV_REQUIRES ${priv_req})

if(CONFIG_APP_WIFI_SHOW_DEMO_INTRO_TEXT)
    target_compile_definitions(${COMPONENT_TARGET} PRIVATE "-D RMAKER_DEMO_PROJECT_NAME=\"${CMAKE_PROJECT_NAME}\"")
endif()

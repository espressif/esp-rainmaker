idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       PRIV_REQUIRES esp_timer freertos mbedtls nvs_flash
                       REQUIRES esp_rainmaker rmaker_user_api)

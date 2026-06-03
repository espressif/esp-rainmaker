idf_component_register(SRC_DIRS "."
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include"
                       REQUIRES esp_http_client json mbedtls nvs_flash esp_rainmaker rmaker_user_api)

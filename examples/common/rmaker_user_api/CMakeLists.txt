set(srcs "app_rmaker_user_api.c")

if (CONFIG_ENABLE_RM_USER_HELPER_API)
    list(APPEND srcs "app_rmaker_user_helper_api.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "private_include"
                       REQUIRES esp_http_client json mbedtls)

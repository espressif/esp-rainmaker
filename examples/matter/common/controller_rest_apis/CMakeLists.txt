if (CONFIG_RMAKER_REST_API_ENABLED)
    list(APPEND SRCS_LIST "controller_rest_apis.c" "matter_device.c")
    list(APPEND INCLUDE_DIRS_LIST "include")
endif()
idf_component_register(SRCS ${SRCS_LIST}
                       INCLUDE_DIRS ${INCLUDE_DIRS_LIST}
                       REQUIRES mbedtls json_parser esp_http_client json_generator rmaker_common esp_rainmaker)

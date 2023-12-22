set(SRCS_LIST )
set(INCLUDE_DIRS_LIST )

if (CONFIG_CONTROLLER_CUSTOM_CLUSTER_ENABLE)
    list(APPEND SRCS_LIST "matter_controller_cluster.cpp" "matter_controller_device_mgr.cpp")
    list(APPEND INCLUDE_DIRS_LIST "include")
endif()

idf_component_register(SRCS ${SRCS_LIST}
                       INCLUDE_DIRS ${INCLUDE_DIRS_LIST}
                       REQUIRES controller_rest_apis chip esp_matter esp_matter_controller)

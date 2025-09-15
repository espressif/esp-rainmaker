set(PRIV_REQUIRES_LIST device app_reset app_insights app_matter)
set(SRCS ./app_main.cpp ./app_matter_light.cpp ./app_driver.cpp)
set(PRIV_INCLUDE_DIRS ./ )

if (NOT CONFIG_EXAMPLE_USE_RAINMAKER_FABRIC)
    list(APPEND SRCS "network_prov_scheme/protocomm_matter_ble.cpp"
        "network_prov_scheme/network_prov_scheme_matter_ble.cpp"
        "network_prov_scheme/app_network.cpp"
        "matter_cwm/matter_commissioning_window_management_std.cpp"
        "matter_cwm/matter_commissioning_window_management.cpp")
    list(APPEND PRIV_INCLUDE_DIRS "network_prov_scheme/" "matter_cwm/")
    list(APPEND PRIV_REQUIRES_LIST network_provisioning)
endif()

list(APPEND PRIV_INCLUDE_DIRS "$ENV{ESP_MATTER_PATH}/examples/common")

if (CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER)
    list(APPEND SRCS "matter_cwm/dynamic_commissionable_data_provider.cpp")
endif()


idf_component_register(SRCS         ${SRCS}
                      PRIV_INCLUDE_DIRS  ${PRIV_INCLUDE_DIRS}
                      PRIV_REQUIRES      ${PRIV_REQUIRES_LIST})

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

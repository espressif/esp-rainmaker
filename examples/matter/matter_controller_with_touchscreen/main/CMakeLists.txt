set(ldfragments linker.lf)

set(SRC_DIRS_LIST "."
                  "./box"
                  "./box/gui"
                  "./box/gui/image")

set(PRIV_INCLUDE_DIRS_LIST "."
                           "./box"
                           "./box/gui")

if (CONFIG_CUSTOM_COMMISSIONABLE_DATA_PROVIDER)
    list(APPEND SRC_DIRS_LIST "./dynamic_qrcode")
    list(APPEND PRIV_INCLUDE_DIRS_LIST "./dynamic_qrcode")
endif()

idf_component_register(
    SRC_DIRS ${SRC_DIRS_LIST}
    PRIV_INCLUDE_DIRS ${PRIV_INCLUDE_DIRS_LIST}
    LDFRAGMENTS "${ldfragments}")

if(CONFIG_OPENTHREAD_BR_AUTO_UPDATE_RCP)
    add_custom_target(rcp_image_generation ALL
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/../create_ota_image.py
        --rcp-build-dir $ENV{IDF_PATH}/examples/openthread/ot_rcp/build
        --target-file ${CMAKE_CURRENT_BINARY_DIR}/spiffs_image/ot_rcp_0/rcp_image
        )

    spiffs_create_partition_image(rcp_fw ${CMAKE_CURRENT_BINARY_DIR}/spiffs_image FLASH_IN_PROJECT
                                  DEPENDS rcp_image_generation)
endif()

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

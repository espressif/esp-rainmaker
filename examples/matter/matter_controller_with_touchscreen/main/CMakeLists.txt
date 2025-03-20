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

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

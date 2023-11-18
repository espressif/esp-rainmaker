set(ldfragments linker.lf)
idf_component_register(
    SRC_DIRS
        "."
        "./box"
        "./box/gui"
        "./box/gui/font"
        "./box/gui/image"
    PRIV_INCLUDE_DIRS
        "."
        "./box"
        "./box/gui"
    LDFRAGMENTS "${ldfragments}")

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)

target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

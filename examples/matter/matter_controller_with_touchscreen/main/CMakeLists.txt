set(ldfragments linker.lf)

set(SRCS_DIRS_LIST "."
                   "./devices"
                   "./box"
                   "./box/gui"
                   "./box/gui/image")

set(PRIV_INCLUDE_DIRS_LIST "."
                           "./box"
                           "./box/gui")

idf_component_register(SRC_DIRS ${SRCS_DIRS_LIST}
                       PRIV_INCLUDE_DIRS ${PRIV_INCLUDE_DIRS_LIST}
                       LDFRAGMENTS "${ldfragments}")


set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

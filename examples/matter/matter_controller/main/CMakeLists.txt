set(ldfragments linker.lf)
idf_component_register(SRCS         ./app_main.cpp ./app_matter_controller.cpp ./app_driver.cpp
                      PRIV_INCLUDE_DIRS  "."
                      LDFRAGMENTS "${ldfragments}")

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

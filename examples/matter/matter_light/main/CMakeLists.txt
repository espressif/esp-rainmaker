set(PRIV_REQUIRES_LIST device app_reset app_insights app_matter)
idf_component_register(SRCS         ./app_main.cpp ./app_matter_light.cpp ./app_driver.cpp
                      PRIV_INCLUDE_DIRS  "."
                      PRIV_REQUIRES      ${PRIV_REQUIRES_LIST})

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

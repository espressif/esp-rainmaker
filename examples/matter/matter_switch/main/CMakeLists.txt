set(PRIV_REQUIRES_LIST device esp_matter esp_matter_console esp_matter_rainmaker app_reset
                       esp_rainmaker app_insights)

idf_component_register(SRCS         ./app_main.cpp ./app_matter.cpp ./app_driver.cpp
                      PRIV_INCLUDE_DIRS  "."
                      PRIV_REQUIRES      ${PRIV_REQUIRES_LIST})

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

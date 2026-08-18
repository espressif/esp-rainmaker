idf_component_register(SRCS "app_controller.cpp"
                            "app_controller_console.cpp"
                            "app_controller_op_creds_issuer.cpp"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "priv_include")

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")

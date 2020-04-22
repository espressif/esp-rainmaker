idf_component_register(SRCS ./app_driver.c ./app_main.c
                       INCLUDE_DIRS ".")

target_add_binary_data(${COMPONENT_TARGET} "raw_github_server.crt" TEXT)

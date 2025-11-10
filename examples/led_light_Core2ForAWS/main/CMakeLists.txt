idf_component_register(SRCS ./app_driver.c ./app_main.c ./sk6812.c ./axp192.c ./axp192_i2c.c ./i2c_device.c
                       INCLUDE_DIRS ".")

target_add_binary_data(${COMPONENT_TARGET} "server.crt" TEXT)

set(component_srcs "src/esp_rmaker_console.c"
                    "src/esp_rmaker_commands.c")

idf_component_register(SRCS "${component_srcs}"
                       INCLUDE_DIRS "include"
                       PRIV_INCLUDE_DIRS "src"
                       REQUIRES "console nvs_flash esp_rmaker_core")


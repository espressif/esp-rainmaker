# Define a list of private dependencies here that include individual rainmaker components
set(PRIV_REQUIRES_LIST FastLED m5_relay_pck m5_lvgl_pck lvgl__lvgl app_network app_insights json_parser)

idf_component_register(SRCS   
                         ./app_driver.cpp ./app_main.cpp  
                         ./interface_module/m5_interface.cpp
                         ./image_module/rainmaker_icon.cpp   
                         ./image_module/light_bulb.cpp
                         ./neohex_module/m5_neohex.cpp
                         ./button_module/m5_button.cpp
                         ./relay_module/m5_relay.cpp
                         INCLUDE_DIRS "."
                                    "../managed_components/lvgl__lvgl/src/libs/qrcode"
                         PRIV_REQUIRES ${PRIV_REQUIRES_LIST})
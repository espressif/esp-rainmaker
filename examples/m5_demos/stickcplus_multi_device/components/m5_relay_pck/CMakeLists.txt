cmake_minimum_required(VERSION 3.5)
idf_component_register(SRCS ./m5_relay/src/Unit_4RELAY.cpp 
                    INCLUDE_DIRS "." 
                    REQUIRES arduino-esp32)

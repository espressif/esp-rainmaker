cmake_minimum_required(VERSION 3.5)
idf_component_register(SRCS ./m5_lvgl/src/utility/lvgl_port_m5stack.cpp
    INCLUDE_DIRS "." "m5_lvgl/src/utility"
    REQUIRES m5stack__m5gfx lvgl__lvgl)

# target_compile_options(${COMPONENT_LIB} PUBLIC -DLVGL_USE_V8=1)
target_compile_options(${COMPONENT_LIB} PUBLIC -DLVGL_USE_V9=1)

# Suppress the return-type warning error
target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-error=return-type)

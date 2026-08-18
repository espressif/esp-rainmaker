# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(DEFINED ENV{RMAKER_PATH})
    set(RMAKER_PATH $ENV{RMAKER_PATH})
else()
    set(RMAKER_PATH ${CMAKE_CURRENT_LIST_DIR}/../../..)
endif(DEFINED ENV{RMAKER_PATH})

# This should be done before using the IDF_TARGET variable.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    "${RMAKER_PATH}/examples/matter/common/app_controller"
    ${extra_components_dirs_append})

add_compile_options(-fdiagnostics-color=always)

idf_build_set_property(__BUILD_COMPONENT_DEPGRAPH_ENABLED 1)

project(matter_controller_with_touchscreen)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H" APPEND)
# TODO: remove the -DCONFIG_CHIP_ENABLE_PAIRING_AUTOSTART workaround in later esp-matter component update
idf_build_set_property(CXX_COMPILE_OPTIONS "-DCONFIG_CHIP_ENABLE_PAIRING_AUTOSTART=0" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
# For RISCV chips, project_include.cmake sets -Wno-format, but does not clear various
# flags that depend on -Wformat
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security;-Wformat=0" APPEND)

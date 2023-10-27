# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(DEFINED ENV{RMAKER_PATH})
    set(RMAKER_PATH $ENV{RMAKER_PATH})
else()
    set(RMAKER_PATH ${CMAKE_CURRENT_LIST_DIR}/../../..)
endif(DEFINED ENV{RMAKER_PATH})

if(NOT DEFINED ENV{ESP_MATTER_PATH})
    message(FATAL_ERROR "Please set ESP_MATTER_PATH to the path of esp-matter repo")
endif(NOT DEFINED ENV{ESP_MATTER_PATH})

if(NOT DEFINED ENV{ESP_MATTER_DEVICE_PATH})
    if("${IDF_TARGET}" STREQUAL "esp32" OR "${IDF_TARGET}" STREQUAL "")
        set(ENV{ESP_MATTER_DEVICE_PATH} $ENV{ESP_MATTER_PATH}/device_hal/device/esp32_devkit_c)
    elseif("${IDF_TARGET}" STREQUAL "esp32c3")
        set(ENV{ESP_MATTER_DEVICE_PATH} $ENV{ESP_MATTER_PATH}/device_hal/device/esp32c3_devkit_m)
    elseif("${IDF_TARGET}" STREQUAL "esp32s3")
        set(ENV{ESP_MATTER_DEVICE_PATH} $ENV{ESP_MATTER_PATH}/device_hal/device/esp32s3_devkit_c)
    elseif("${IDF_TARGET}" STREQUAL "esp32c6")
        set(ENV{ESP_MATTER_DEVICE_PATH} $ENV{ESP_MATTER_PATH}/device_hal/device/esp32c6_devkit_c)
    else()
        message(FATAL_ERROR "Unsupported IDF_TARGET")
    endif()
endif(NOT DEFINED ENV{ESP_MATTER_DEVICE_PATH})

set(ESP_MATTER_PATH $ENV{ESP_MATTER_PATH})
set(MATTER_SDK_PATH ${ESP_MATTER_PATH}/connectedhomeip/connectedhomeip)

# This should be done before using the IDF_TARGET variable.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include($ENV{ESP_MATTER_DEVICE_PATH}/esp_matter_device.cmake)

idf_build_set_property(RAINMAKER_ENABLED 1)

set(EXTRA_COMPONENT_DIRS
    "${MATTER_SDK_PATH}/config/esp32/components"
    "${ESP_MATTER_PATH}/components"
    "${ESP_MATTER_PATH}/device_hal/device"
    "${ESP_MATTER_PATH}/examples/common"
    "${RMAKER_PATH}/examples/common/app_insights"
    "${RMAKER_PATH}/components"
    ${extra_components_dirs_append})

# Include insights only for IDF <= v4.4 as it is already included by esp-matter for >= v5.0
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}" VERSION_LESS_EQUAL "4.4")
    list(APPEND EXTRA_COMPONENT_DIRS "${RMAKER_PATH}/components/esp-insights/components")
endif()

project(matter_light)

idf_build_set_property(CXX_COMPILE_OPTIONS "-std=gnu++17;-Os;-DCHIP_HAVE_CONFIG_H" APPEND)
idf_build_set_property(C_COMPILE_OPTIONS "-Os" APPEND)
# For RISCV chips, project_include.cmake sets -Wno-format, but does not clear various
# flags that depend on -Wformat
idf_build_set_property(COMPILE_OPTIONS "-Wno-format-nonliteral;-Wno-format-security" APPEND)

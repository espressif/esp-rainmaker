cmake_minimum_required(VERSION 3.5)

# Set KVS_SDK_PATH if not already set in environment
if(NOT DEFINED ENV{KVS_SDK_PATH})
    message(FATAL_ERROR "KVS_SDK_PATH not set in environment. Please set KVS_SDK_PATH to your local clone of esp-port-for-amazon-kvs-sdk")
endif()

set(PROJECT_VER "1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(standalone_camera)

# P4 slave-enabled overlays (p4_eye/p4x_eye/p4_c5) pack the co-processor image
# into a 'slave' SPIFFS partition; slave_flasher writes it to the C6/C5 at boot.
# Fail fast if the images are missing rather than flashing an empty partition.
if(CONFIG_SLAVE_FLASHER_ENABLE)
    set(_slave_fw_dir "${CMAKE_CURRENT_SOURCE_DIR}/target-firmware")
    set(_missing "")
    foreach(_bin bootloader.bin partition-table.bin app.bin)
        if(NOT EXISTS "${_slave_fw_dir}/${_bin}")
            list(APPEND _missing "${_bin}")
        endif()
    endforeach()
    if(_missing)
        message(FATAL_ERROR
            "SLAVE_FLASHER_ENABLE is on but these C6/C5 image files are missing "
            "from target-firmware/: ${_missing}.\n"
            "Build the network_adapter example for the co-processor target, then "
            "copy bootloader.bin, partition-table.bin and app.bin into "
            "${_slave_fw_dir}/ before re-running this build. "
            "See target-firmware/README.md.")
    endif()
    spiffs_create_partition_image(slave ${_slave_fw_dir} FLASH_IN_PROJECT)
endif()

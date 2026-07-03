cmake_minimum_required(VERSION 3.5)

# Set KVS_SDK_PATH if not already set in environment
if(NOT DEFINED ENV{KVS_SDK_PATH})
    message(FATAL_ERROR "KVS_SDK_PATH not set in environment. Please set KVS_SDK_PATH to your local clone of esp-port-for-amazon-kvs-sdk")
endif()

set(PROJECT_VER "1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(standalone_camera)

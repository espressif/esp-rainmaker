cmake_minimum_required(VERSION 3.5)

# Set KVS_SDK_PATH if not already set in environment
if(NOT DEFINED ENV{KVS_SDK_PATH})
    message(FATAL_ERROR "KVS_SDK_PATH not set in environment. Please set KVS_SDK_PATH to your local clone of amazon-kinesis-video-streams-webrtc-sdk-c")
endif()

# Enable signaling only mode (This lets webrtc_bridge to what role to play)
option(ENABLE_SIGNALLING_ONLY "enable only the signalling component" ON)
option(ENABLE_STREAMING_ONLY "enable only the streaming component" OFF)

set(PROJECT_VER "1.0")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(COMPONENTS main)

project(rmaker_split_camera)

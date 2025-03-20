set(SRC_DIRS_LIST ".")

set(INCLUDE_DIRS_LIST ".")

if (CONFIG_M5STACK_THREAD_BR_BOARD)
    list(APPEND SRC_DIRS_LIST "./m5stack_display"
                              "./m5stack_display/icon")
    list(APPEND INCLUDE_DIRS_LIST "./m5stack_display")
endif()

idf_component_register(SRC_DIRS ${SRC_DIRS_LIST}
                       INCLUDE_DIRS ${INCLUDE_DIRS_LIST})

list(APPEND SRCS_LIST "app_matter.cpp")
list(APPEND INCLUDE_DIRS_LIST "include")

idf_component_register(SRCS ${SRCS_LIST}
                        INCLUDE_DIRS ${INCLUDE_DIRS_LIST}
                        REQUIRES esp_matter esp_matter_rainmaker
                        )

idf_component_register(SRCS ./json_parser.c ./jsmn/src/jsmn-changed.c
                       INCLUDE_DIRS ./ ./jsmn/include
                       REQUIRES
                       PRIV_REQUIRES )


COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_SRCDIRS := .
ifndef CONFIG_WS2812_LED_ENABLE
    COMPONENT_OBJEXCLUDE += led_strip_rmt_ws2812.o
endif
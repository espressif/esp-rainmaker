#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

ifeq ($(HOMEKIT_PATH),)
    $(error Please set HOMEKIT_PATH to esp-homekit-sdk repo)
endif

PROJECT_NAME := homekit_switch
PROJECT_VER := 1.0

# Add RainMaker components and other common application components
EXTRA_COMPONENT_DIRS += $(PROJECT_PATH)/../../components $(PROJECT_PATH)/../common $(HOMEKIT_PATH)/components/homekit

include $(IDF_PATH)/make/project.mk

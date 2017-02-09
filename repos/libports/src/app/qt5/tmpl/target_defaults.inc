# build with QtCore and QtGui support by default
# - can be overridden in the qmake project file

QT = core gui

# find out the name of the project directory

PROJECT_DIR_NAME = $(notdir $(abspath $(PRG_DIR)))

# include the qmake project file
# - if the qmake project file has a different name than the project directory,
#   set QMAKE_PROJECT_FILE in target.mk before the inclusion of this file

QMAKE_PROJECT_FILE ?= $(realpath $(PRG_DIR)/$(PROJECT_DIR_NAME).pro)

ifneq ($(call select_from_ports,qoost),)
ifneq ($(call select_from_ports,qt5),)
ifneq ($(strip $(QMAKE_PROJECT_FILE)),)
include $(QMAKE_PROJECT_FILE)
endif
endif
endif

# how to name the generated executable
# (if not already defined in the qmake project file)
# - can be overridden in target.mk after inclusion of this file

ifndef TARGET
TARGET = $(PROJECT_DIR_NAME)
endif

# default stack size of the main thread
# - can be overridden anywhere in target.mk

QT_MAIN_STACK_SIZE ?= 512*1024

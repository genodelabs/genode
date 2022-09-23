CBE_DIR := $(call select_from_ports,cbe)

LIBS += spark cbe_common

INC_DIR += $(CBE_DIR)/src/lib/cbe_common
INC_DIR += $(CBE_DIR)/src/lib/cbe_cxx_common

SRC_ADB += cbe-cxx.adb

SRC_CC += print_cstring.cc

vpath % $(CBE_DIR)/src/lib/cbe_cxx_common

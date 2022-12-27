CBE_DIR := $(call select_from_ports,cbe)

LIBS    += spark sha256_4k cbe_common

INC_DIR += $(CBE_DIR)/src/lib/cbe_check

SRC_ADB += cbe-check_library.adb
SRC_ADB += cbe-superblock_check.adb
SRC_ADB += cbe-vbd_check.adb
SRC_ADB += cbe-free_tree_check.adb

vpath % $(CBE_DIR)/src/lib/cbe_check

CC_ADA_OPT += -gnatec=$(CBE_DIR)/src/lib/cbe_check/pragmas.adc

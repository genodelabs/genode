CBE_DIR := $(call select_from_ports,cbe)

LIBS    += spark sha256_4k cbe_common

INC_DIR += $(CBE_DIR)/src/lib/cbe_dump

SRC_ADB += cbe-dump_library.adb
SRC_ADB += cbe-superblock_dump.adb
SRC_ADB += cbe-vbd_dump.adb
SRC_ADB += cbe-free_tree_dump.adb

vpath % $(CBE_DIR)/src/lib/cbe_dump

CC_ADA_OPT += -gnatec=$(CBE_DIR)/src/lib/cbe_dump/pragmas.adc

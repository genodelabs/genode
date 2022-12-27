CBE_DIR := $(call select_from_ports,cbe)

LIBS    += spark sha256_4k cbe_common cbe_cxx_common

INC_DIR += $(CBE_DIR)/src/lib/cbe_init

SRC_ADB += cbe-init_library.adb
SRC_ADB += cbe-block_allocator.adb
SRC_ADB += cbe-superblock_initializer.adb
SRC_ADB += cbe-vbd_initializer.adb
SRC_ADB += cbe-free_tree_initializer.adb

vpath % $(CBE_DIR)/src/lib/cbe_init

CC_ADA_OPT += -gnatec=$(CBE_DIR)/src/lib/cbe_init/pragmas.adc

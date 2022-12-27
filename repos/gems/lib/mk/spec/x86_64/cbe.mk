CBE_DIR := $(call select_from_ports,cbe)

LIBS    += spark sha256_4k cbe_common

INC_DIR += $(CBE_DIR)/src/lib/cbe

SRC_ADB += cbe-library.adb
SRC_ADB += cbe-request_pool.adb
SRC_ADB += cbe-crypto.adb
SRC_ADB += cbe-tree_helper.adb
SRC_ADB += cbe-translation.adb
SRC_ADB += cbe-cache.adb
SRC_ADB += cbe-new_free_tree.adb
SRC_ADB += cbe-ft_resizing.adb
SRC_ADB += cbe-mt_resizing.adb
SRC_ADB += cbe-meta_tree.adb
SRC_ADB += cbe-generic_index_queue.adb
SRC_ADB += cbe-superblock_control.adb
SRC_ADB += cbe-vbd_rekeying.adb

vpath % $(CBE_DIR)/src/lib/cbe

CC_ADA_OPT += -gnatec=$(CBE_DIR)/src/lib/cbe/pragmas.adc

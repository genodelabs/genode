TRESOR_DIR := $(REP_DIR)/src/lib/tresor

SRC_CC += crypto.cc
SRC_CC += request_pool.cc
SRC_CC += hash.cc
SRC_CC += trust_anchor.cc
SRC_CC += block_io.cc
SRC_CC += meta_tree.cc
SRC_CC += virtual_block_device.cc
SRC_CC += superblock_control.cc
SRC_CC += free_tree.cc
SRC_CC += module.cc
SRC_CC += vbd_initializer.cc
SRC_CC += ft_initializer.cc
SRC_CC += sb_initializer.cc
SRC_CC += sb_check.cc
SRC_CC += vbd_check.cc
SRC_CC += ft_check.cc

vpath % $(TRESOR_DIR)

INC_DIR += $(TRESOR_DIR)/include

LIBS += libcrypto
LIBS += vfs

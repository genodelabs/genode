REQUIRES = conversion_to_vfs_plugin

FUSE_EXT2_PORT_DIR := $(call select_from_ports,fuse-ext2)
FUSE_EXT2_DIR      := $(FUSE_EXT2_PORT_DIR)/src/lib/fuse-ext2/fuse-ext2

FILTER_OUT = fuse-ext2.probe.c fuse-ext2.wait.c
SRC_C  = $(filter-out $(FILTER_OUT), $(notdir $(wildcard $(FUSE_EXT2_DIR)/*.c)))
SRC_CC = init.cc

LIBS   = libc libc_fuse libfuse libext2fs

CC_OPT = -DHAVE_CONFIG_H -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64

CC_CXX_OPT +=-fpermissive

INC_DIR += $(REP_DIR)/src/lib/fuse-ext2 \
           $(FUSE_EXT2_DIR) \
           $(FUSE_EXT2_PORT_DIR)/include/fuse-ext2

vpath %.c $(FUSE_EXT2_DIR)
vpath %.cc $(REP_DIR)/src/lib/fuse-ext2

SHARED_LIB = yes

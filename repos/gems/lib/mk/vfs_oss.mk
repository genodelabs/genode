VFS_DIR := $(REP_DIR)/src/lib/vfs/oss

SRC_CC := vfs.cc

LD_OPT += --version-script=$(VFS_DIR)/symbol.map

SHARED_LIB := yes

vpath %.cc $(VFS_DIR)

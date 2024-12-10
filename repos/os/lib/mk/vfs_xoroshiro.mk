VFS_DIR := $(REP_DIR)/src/lib/vfs/xoroshiro

SRC_CC := vfs.cc

# for base/internal/xoroshiro.h
REP_INC_DIR += src/include/base/internal

LD_OPT += --version-script=$(VFS_DIR)/symbol.map

SHARED_LIB := yes

vpath %.cc $(VFS_DIR)

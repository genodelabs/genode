SHARED_LIB = yes

VFS_DIR := $(call select_from_repositories,src/lib/vfs/ip)
LIBS     = lwip
SRC_CC   = vfs.cc vfs_lwip.cc
LD_OPT  += --version-script=$(VFS_DIR)/symbol.map

CC_OPT += -Wno-error=missing-field-initializers
CC_OPT += -Wno-missing-field-initializers

vpath %.cc $(VFS_DIR)

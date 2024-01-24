SHARED_LIB = yes

VFS_DIR  = $(REP_DIR)/src/lib/vfs/lxip
LIBS     = lxip format
SRC_CC   = vfs.cc
LD_OPT  += --version-script=$(VFS_DIR)/symbol.map

CC_OPT += -Wno-error=missing-field-initializers
CC_OPT += -Wno-missing-field-initializers

vpath %.cc $(VFS_DIR)

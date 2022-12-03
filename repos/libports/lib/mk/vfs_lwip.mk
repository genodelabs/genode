SRC_CC = vfs.cc printf.cc rand.cc sys_arch.cc

VFS_DIR  = $(REP_DIR)/src/lib/vfs/lwip
INC_DIR += $(VFS_DIR)

REP_INC_DIR += src/lib/lwip/include

LD_OPT  += --version-script=$(VFS_DIR)/symbol.map

LIBS += lwip

vpath %.cc $(VFS_DIR)

SHARED_LIB = yes

CC_CXX_WARN_STRICT_CONVERSION =

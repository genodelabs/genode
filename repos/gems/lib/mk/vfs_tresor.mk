LIB_DIR := $(REP_DIR)/src/lib/vfs/tresor

SRC_CC := vfs.cc

INC_DIR += $(LIB_DIR)

vpath % $(LIB_DIR)

LIBS += tresor

SHARED_LIB := yes

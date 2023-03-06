SRC_CC   = vfs_rump.cc random.cc
LIBS     = rump rump_fs format
INC_DIR += $(REP_DIR)/src/lib/vfs/rump
INC_DIR += $(REP_DIR)/src/include
vpath %.cc $(REP_DIR)/src/lib/vfs/rump

SHARED_LIB = yes

CC_CXX_WARN_STRICT =

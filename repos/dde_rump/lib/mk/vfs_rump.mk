SRC_CC   = vfs_rump.cc random.cc
LIBS     = rump rump_fs
INC_DIR += $(REP_DIR)/src/lib/vfs/rump
vpath %.cc $(REP_DIR)/src/lib/vfs/rump

SHARED_LIB = yes

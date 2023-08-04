SRC_CC += vfs.cc
SRC_CC += memcopy.cc

vpath %.cc $(REP_DIR)/src/lib/vfs/tresor_crypto

INC_DIR += $(REP_DIR)/src/lib/vfs/tresor_crypto
INC_DIR += $(REP_DIR)/src/lib/tresor/include

SHARED_LIB = yes

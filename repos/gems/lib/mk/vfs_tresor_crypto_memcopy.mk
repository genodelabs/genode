SRC_CC := vfs.cc
SRC_CC += memcopy.cc

INC_DIR += $(REP_DIR)/src/lib/tresor/include

vpath vfs.cc $(REP_DIR)/src/lib/vfs/tresor_crypto/
vpath %.cc   $(REP_DIR)/src/lib/vfs/tresor_crypto/memcopy

SHARED_LIB = yes

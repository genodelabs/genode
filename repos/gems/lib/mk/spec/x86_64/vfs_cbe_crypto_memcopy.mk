SRC_CC := vfs.cc
SRC_CC += memcopy.cc

vpath vfs.cc $(REP_DIR)/src/lib/vfs/cbe_crypto/
vpath %.cc   $(REP_DIR)/src/lib/vfs/cbe_crypto/memcopy

SHARED_LIB = yes

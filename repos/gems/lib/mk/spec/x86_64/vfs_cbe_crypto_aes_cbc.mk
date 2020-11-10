SRC_CC := vfs.cc
SRC_CC += aes_cbc.cc

LIBS += aes_cbc_4k

vpath vfs.cc $(REP_DIR)/src/lib/vfs/cbe_crypto/
vpath %      $(REP_DIR)/src/lib/vfs/cbe_crypto/aes_cbc

SHARED_LIB = yes

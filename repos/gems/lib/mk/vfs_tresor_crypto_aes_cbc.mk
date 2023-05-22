SRC_CC := vfs.cc
SRC_CC += aes_cbc.cc

INC_DIR += $(REP_DIR)/src/lib/tresor/include

LIBS += aes_cbc_4k

vpath vfs.cc $(REP_DIR)/src/lib/vfs/tresor_crypto/
vpath %      $(REP_DIR)/src/lib/vfs/tresor_crypto/aes_cbc

SHARED_LIB = yes

OPENSSL_DIR = $(call select_from_ports,openssl)

SRC_CC = vfs.cc

INC_DIR += $(OPENSSL_DIR)/include

LIBS += libcrypto

vpath % $(REP_DIR)/src/lib/vfs/cbe_trust_anchor

SHARED_LIB := yes

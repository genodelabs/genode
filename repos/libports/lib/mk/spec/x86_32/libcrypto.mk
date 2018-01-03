# has to be the first path because it includes openssl/opensslconf.h
INC_DIR += $(REP_DIR)/src/lib/openssl/spec/x86_32

CC_OPTS += -DL_ENDIAN

include $(REP_DIR)/lib/mk/libcrypto.inc

CC_CXX_WARN_STRICT =

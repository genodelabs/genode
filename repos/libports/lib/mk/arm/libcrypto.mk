# has to be the first path because it includes openssl/opensslconf.h
INC_DIR += $(REP_DIR)/src/lib/openssl/arm

CC_OPTS += -DL_ENDIAN

include $(REP_DIR)/lib/mk/libcrypto.inc

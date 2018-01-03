# has to be the first path because it includes openssl/opensslconf.h
INC_DIR += $(REP_DIR)/src/lib/openssl/spec/x86_64

CC_OPTS += -DL_ENDIAN

SRC_S += modexp512.s
SRC_S += rc4_md5.s

vpath %.s $(call select_from_ports,openssl)/src/lib/openssl/x86_64

include $(REP_DIR)/lib/mk/libcrypto.inc

CC_CXX_WARN_STRICT =

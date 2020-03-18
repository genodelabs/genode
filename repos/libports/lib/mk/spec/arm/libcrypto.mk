# has to be the first path because it includes openssl/opensslconf.h
INC_DIR += $(REP_DIR)/src/lib/openssl/spec/32bit

CC_OPT += -DOPENSSL_CPUID_OBJ -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_GF2m
CC_OPT += -DSHA1_ASM -DSHA256_ASM -DSHA512_ASM -DKECCAK1600_ASM -DAES_ASM
CC_OPT += -DGHASH_ASM -DECP_NISTZ256_ASM -DPOLY1305_ASM

SRC_C = \
	armcap.c \
	bf/bf_enc.c \
	bn/bn_asm.c \
	camellia/cmll_cbc.c \
	camellia/cmll_misc.c \
	des/des_enc.c \
	# end of SRC_C

SRC_S = \
	aes/asm/aes-armv4.S \
	aes/asm/aesv8-arm32.S \
	armv4cpuid.S \
	bn/asm/armv4-gf2m.S \
	bn/asm/armv4-mont.S \
	chacha/asm/chacha-armv4.S \
	ec/asm/ecp_nistz256-armv4.S \
	modes/asm/ghash-armv4.S \
	modes/asm/ghashv8-arm32.S \
	poly1305/asm/poly1305-armv4.S \
	sha/asm/keccak1600-armv4.S \
	sha/asm/sha1-armv4-large.S \
	sha/asm/sha256-armv4.S \
	sha/asm/sha512-armv4.S \
	# end of SRC_S

vpath %.S $(call select_from_ports,openssl)/src/lib/openssl/crypto

include $(REP_DIR)/lib/mk/libcrypto.inc

CC_CXX_WARN_STRICT =

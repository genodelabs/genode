# has to be the first path because it includes openssl/opensslconf.h
INC_DIR += $(REP_DIR)/src/lib/openssl/spec/64bit

CC_OPT += -DOPENSSL_CPUID_OBJ -DOPENSSL_BN_ASM_MONT -DSHA1_ASM -DSHA256_ASM
CC_OPT += -DSHA512_ASM -DKECCAK1600_ASM -DVPAES_ASM -DECP_NISTZ256_ASM
CC_OPT += -DPOLY1305_ASM

SRC_C = \
	armcap_genode.c \
	aes/aes_core.c \
	bf/bf_enc.c \
	bn/bn_asm.c \
	camellia/cmll_cbc.c \
	camellia/cmll_misc.c \
	des/des_enc.c \
	# end of SRC_C

SRC_S = \
	aes/asm/aesv8-arm64.S \
	aes/asm/vpaes-armv8.S \
	arm64cpuid.S \
	bn/asm/armv8-mont.S \
	chacha/asm/chacha-armv8.S \
	ec/asm/ecp_nistz256-armv8.S \
	modes/asm/ghashv8-arm64.S \
	poly1305/asm/poly1305-armv8.S \
	sha/asm/keccak1600-armv8.S \
	sha/asm/sha1-armv8.S \
	sha/asm/sha256-armv8.S \
	sha/asm/sha512-armv8.S \
	# end of SRC_S

vpath %.S $(call select_from_ports,openssl)/src/lib/openssl/crypto

ifeq ($(filter-out $(SPECS),neon),)
	vpath armcap_genode.c $(REP_DIR)/src/lib/openssl/crypto/spec/neon
else
	vpath armcap_genode.c $(REP_DIR)/src/lib/openssl/crypto/spec/arm
endif

include $(REP_DIR)/lib/mk/libcrypto.inc

CC_CXX_WARN_STRICT =

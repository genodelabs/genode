# has to be the first path because it includes openssl/opensslconf.h
INC_DIR += $(REP_DIR)/src/lib/openssl/spec/64bit

CC_OPT += -DOPENSSL_CPUID_OBJ -DOPENSSL_IA32_SSE2 -DOPENSSL_BN_ASM_MONT
CC_OPT += -DOPENSSL_BN_ASM_MONT5 -DOPENSSL_BN_ASM_GF2m -DSHA1_ASM -DSHA256_ASM
CC_OPT += -DSHA512_ASM -DKECCAK1600_ASM -DRC4_ASM -DMD5_ASM -DAESNI_ASM -DVPAES_ASM
CC_OPT += -DGHASH_ASM -DECP_NISTZ256_ASM -DX25519_ASM -DPOLY1305_ASM

SRC_C = \
	aes/aes_core.c \
	bf/bf_enc.c \
	bn/asm/x86_64-gcc.c \
	camellia/cmll_misc.c \
	des/des_enc.c \
	# end of SRC_C

SRC_S = \
	aes/asm/aesni-mb-x86_64.s \
	aes/asm/aesni-sha1-x86_64.s \
	aes/asm/aesni-sha256-x86_64.s \
	aes/asm/aesni-x86_64.s \
	aes/asm/vpaes-x86_64.s \
	bn/asm/rsaz-avx2.s \
	bn/asm/rsaz-x86_64.s \
	bn/asm/x86_64-gf2m.s \
	bn/asm/x86_64-mont.s \
	bn/asm/x86_64-mont5.s \
	camellia/asm/cmll-x86_64.s \
	chacha/asm/chacha-x86_64.s \
	ec/asm/ecp_nistz256-x86_64.s \
	ec/asm/x25519-x86_64.s \
	md5/asm/md5-x86_64.s \
	modes/asm/aesni-gcm-x86_64.s \
	modes/asm/ghash-x86_64.s \
	poly1305/asm/poly1305-x86_64.s \
	rc4/asm/rc4-md5-x86_64.s \
	rc4/asm/rc4-x86_64.s \
	sha/asm/keccak1600-x86_64.s \
	sha/asm/sha1-mb-x86_64.s \
	sha/asm/sha1-x86_64.s \
	sha/asm/sha256-mb-x86_64.s \
	sha/asm/sha256-x86_64.s \
	sha/asm/sha512-x86_64.s \
	whrlpool/asm/wp-x86_64.s \
	x86_64cpuid.s \
	# end of SRC_S

vpath %.s $(call select_from_ports,openssl)/src/lib/openssl/crypto

include $(REP_DIR)/lib/mk/libcrypto.inc

CC_CXX_WARN_STRICT =

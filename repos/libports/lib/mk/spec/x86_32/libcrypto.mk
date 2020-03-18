# has to be the first path because it includes openssl/opensslconf.h
INC_DIR += $(REP_DIR)/src/lib/openssl/spec/32bit

CC_OPT += -DOPENSSL_CPUID_OBJ -DOPENSSL_BN_ASM_PART_WORDS -DOPENSSL_IA32_SSE2
CC_OPT += -DOPENSSL_BN_ASM_MONT -DOPENSSL_BN_ASM_GF2m -DSHA1_ASM -DSHA256_ASM
CC_OPT += -DSHA512_ASM -DRC4_ASM -DMD5_ASM -DRMD160_ASM -DAESNI_ASM -DVPAES_ASM
CC_OPT += -DWHIRLPOOL_ASM -DGHASH_ASM -DECP_NISTZ256_ASM -DPOLY1305_ASM

SRC_C = \
	aes/aes_core.c \
	# end of SRC_C

SRC_S = \
	aes/asm/aesni-x86.s \
	aes/asm/vpaes-x86.s \
	bf/asm/bf-586.s \
	bn/asm/bn-586.s \
	bn/asm/co-586.s \
	bn/asm/x86-gf2m.s \
	bn/asm/x86-mont.s \
	camellia/asm/cmll-x86.s \
	chacha/asm/chacha-x86.s \
	des/asm/des-586.s \
	ec/asm/ecp_nistz256-x86.s \
	md5/asm/md5-586.s \
	modes/asm/ghash-x86.s \
	poly1305/asm/poly1305-x86.s \
	rc4/asm/rc4-586.s \
	ripemd/asm/rmd-586.s \
	sha/asm/sha1-586.s \
	sha/asm/sha256-586.s \
	sha/asm/sha512-586.s \
	whrlpool/asm/wp-mmx.s \
	x86cpuid.s \
	# end of SRC_S

vpath %.s $(call select_from_ports,openssl)/src/lib/openssl/crypto

include $(REP_DIR)/lib/mk/libcrypto.inc

CC_CXX_WARN_STRICT =

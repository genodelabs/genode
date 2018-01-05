LIBGCRYPT_DIR     := $(call select_from_ports,libgcrypt)/src/lib/libgcrypt
LIBGCRYPT_SRC_DIR := $(LIBGCRYPT_DIR)/src

LIBS := libc libgpg-error

SRC_C := global.c stdmem.c visibility.c fips.c misc.c secmem.c md.c cipher.c \
         random.c random-csprng.c poly1305.c rndjent.c pubkey.c random-drbg.c \
         primegen.c random-system.c sha1.c mac.c hmac-tests.c mac-poly1305.c \
         hwfeatures.c hmac256.c blake2.c rndhw.c hash-common.c sexp.c \
         mac-hmac.c rsa.c rsa-common.c pubkey-util.c sha256.c

SRC_C += $(notdir $(wildcard $(LIBGCRYPT_DIR)/mpi/mpi*.c))
SRC_C += $(notdir $(wildcard $(LIBGCRYPT_DIR)/mpi/generic/*.c))
SRC_C += $(notdir $(wildcard $(LIBGCRYPT_DIR)/cipher/cipher-*.c))

INC_DIR += $(REP_DIR)/src/lib/libgcrypt
INC_DIR += $(REP_DIR)/src/lib/libgcrypt/mpi
INC_DIR += $(LIBGCRYPT_SRC_DIR)
INC_DIR += $(LIBGCRYPT_DIR)/mpi
INC_DIR += $(call select_from_ports,libgcrypt)/include/libgcrypt

CC_OPT += -D_GCRYPT_IN_LIBGCRYPT
CC_OPT += -DVERSION='"$(< $(LIBGCRYPT_DIR))"'
CC_OPT += -DLIBGCRYPT_CIPHERS='"rsa"'
CC_OPT += -DLIBGCRYPT_PUBKEY_CIPHERS='"rsa"'
CC_OPT += -DLIBGCRYPT_DIGESTS='""'

CC_OPT_global += -Wno-switch

vpath %.c $(LIBGCRYPT_SRC_DIR)
vpath %.c $(LIBGCRYPT_DIR)/cipher
vpath %.c $(LIBGCRYPT_DIR)/random
vpath %.c $(LIBGCRYPT_DIR)/mpi
vpath %.c $(LIBGCRYPT_DIR)/mpi/generic

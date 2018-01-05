content: src/lib/libgcrypt src/lib/libgpg-error include lib/mk lib/import LICENSE

LIBGCRYPT_DIR     := $(call port_dir,$(REP_DIR)/ports/libgcrypt)
LIBGCRYPT_SRC_DIR := $(LIBGCRYPT_DIR)/src/lib/libgcrypt

include:
	cp -r $(LIBGCRYPT_DIR)/include/libgcrypt $@

src/lib/libgcrypt:
	mkdir -p $@
	cp -r $(addprefix $(LIBGCRYPT_SRC_DIR)/,src mpi cipher random compat) $@
	cp $(REP_DIR)/src/lib/libgcrypt/config.h $@
	cp $(REP_DIR)/src/lib/libgcrypt/mod-source-info.h $@
	cp $(REP_DIR)/src/lib/libgcrypt/mpi/mpi-asm-defs.h $@/mpi

src/lib/libgpg-error:
	mkdir -p $@
	cp -r $(LIBGCRYPT_DIR)/src/lib/libgpg-error/src $@
	cp $(REP_DIR)/src/lib/libgpg-error/config.h $@

lib/mk:
	mkdir -p $@
	cp $(addprefix $(REP_DIR)/lib/mk/,libgpg-error.mk libgcrypt.mk) $@

lib/import:
	mkdir -p $@
	cp $(REP_DIR)/lib/import/import-libgcrypt.mk $@

LICENSE:
	cp $(LIBGCRYPT_SRC_DIR)/COPYING $@


BUILD_LIBS := \
	cbe_cxx \
	cbe_init_cxx \
	cbe_dump_cxx \
	cbe_check_cxx

MIRROR_FROM_CBE_DIR := \
	src/lib/cbe \
	src/lib/cbe_common \
	src/lib/cbe_cxx \
	src/lib/cbe_cxx_common \
	src/lib/cbe_init \
	src/lib/cbe_init_cxx \
	src/lib/cbe_dump \
	src/lib/cbe_dump_cxx \
	src/lib/cbe_check \
	src/lib/cbe_check_cxx \
	src/lib/sha256_4k

MIRROR_FROM_REP_DIR := \
	lib/import/import-cbe.mk \
	lib/import/import-cbe_common.mk \
	lib/import/import-cbe_init.mk \
	lib/import/import-cbe_dump.mk \
	lib/import/import-cbe_check.mk \
	lib/import/import-sha256_4k.mk \
	lib/mk/spec/x86_64/cbe.mk \
	lib/mk/spec/x86_64/cbe_common.mk \
	lib/mk/spec/x86_64/cbe_cxx.mk \
	lib/mk/spec/x86_64/cbe_cxx_common.mk \
	lib/mk/spec/x86_64/cbe_init.mk \
	lib/mk/spec/x86_64/cbe_init_cxx.mk \
	lib/mk/spec/x86_64/cbe_dump.mk \
	lib/mk/spec/x86_64/cbe_dump_cxx.mk \
	lib/mk/spec/x86_64/cbe_check.mk \
	lib/mk/spec/x86_64/cbe_check_cxx.mk \
	lib/mk/spec/x86_64/vfs_cbe.mk \
	lib/mk/spec/x86_64/vfs_cbe_crypto_aes_cbc.mk \
	lib/mk/spec/x86_64/vfs_cbe_crypto_memcopy.mk \
	lib/mk/spec/x86_64/vfs_cbe_trust_anchor.mk \
	lib/mk/generate_ada_main_pkg.inc \
	lib/mk/sha256_4k.mk \
	lib/symbols/cbe_check_cxx \
	lib/symbols/cbe_dump_cxx \
	lib/symbols/cbe_init_cxx \
	src/lib/vfs/cbe \
	src/lib/vfs/cbe_crypto/vfs.cc \
	src/lib/vfs/cbe_crypto/aes_cbc \
	src/lib/vfs/cbe_crypto/memcopy \
	src/lib/vfs/cbe_trust_anchor \
	src/app/cbe_check \
	src/app/cbe_dump \
	src/app/cbe_init \
	src/app/cbe_init_trust_anchor \
	src/app/cbe_tester \
	include/cbe

CBE_DIR := $(call port_dir,$(REP_DIR)/ports/cbe)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_CBE_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_CBE_DIR):
	mkdir -p $(dir $@)
	cp -r $(CBE_DIR)/$@ $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

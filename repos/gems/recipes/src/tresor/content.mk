MIRROR_FROM_REP_DIR := \
	src/lib/tresor \
	lib/import/import-tresor.mk \
	lib/mk/tresor.mk \
	lib/mk/vfs_tresor.mk \
	lib/mk/vfs_tresor_crypto_aes_cbc.mk \
	lib/mk/vfs_tresor_crypto_memcopy.mk \
	lib/mk/vfs_tresor_trust_anchor.mk \
	src/lib/vfs/tresor \
	src/lib/vfs/tresor_crypto \
	src/lib/vfs/tresor_trust_anchor \
	src/app/tresor_init \
	src/app/tresor_init_trust_anchor \
	src/app/tresor_tester

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

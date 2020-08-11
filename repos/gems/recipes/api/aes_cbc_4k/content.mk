MIRROR_FROM_REP_DIR := \
	include/aes_cbc_4k \
	src/lib/aes_cbc_4k \
	lib/import/import-aes_cbc_4k.mk \
	lib/mk/aes_cbc_4k.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@


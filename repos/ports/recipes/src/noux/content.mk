MIRROR_FROM_REP_DIR := src/noux src/lib/libc_noux lib/mk/libc_noux.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_LIBPORTS := include/libc-plugin src/lib/libc/libc_mem_alloc.h

content: $(MIRROR_FROM_LIBPORTS)

$(MIRROR_FROM_LIBPORTS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/libports/$@ $@

MIRROR_FROM_OS := include/init/child_policy.h

content: $(MIRROR_FROM_OS)

$(MIRROR_FROM_OS):
	mkdir -p $(dir $@)
	cp -r $(GENODE_DIR)/repos/os/$@ $@

content: LICENSE

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@


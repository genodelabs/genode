MIRROR_FROM_REP_DIR := src/lib/vfs/xoroshiro lib/mk/vfs_xoroshiro.mk

content: $(MIRROR_FROM_REP_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_BASE_DIR := src/include/base/internal/xoroshiro.h

content: $(MIRROR_FROM_BASE_DIR)

$(MIRROR_FROM_BASE_DIR):
	mkdir -p $(dir $@)
	cp -r $(addprefix $(GENODE_DIR)/repos/base/,$@) $(dir $@)

LICENSE:
	cp $(GENODE_DIR)/LICENSE $@

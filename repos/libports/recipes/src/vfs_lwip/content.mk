MIRROR_FROM_REP_DIR := \
	$(shell cd $(REP_DIR); find include/lwip src/lib/lwip  src/lib/vfs/lwip -type f) \
	lib/import/import-lwip.mk \
	lib/mk/lwip.mk \
	lib/mk/vfs_lwip.mk \
	recipes/src/vfs_lwip \

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/lwip)

MIRROR_FROM_PORT_DIR := $(shell cd $(PORT_DIR); find include src -type f)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

LICENSE:
	cp $(PORT_DIR)/src/lib/lwip/lwip-*/COPYING $@

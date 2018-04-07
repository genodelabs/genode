MIRROR_FROM_REP_DIR := lib/import/import-icu.mk \
                       lib/symbols/icu

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: include LICENCE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/icu)

include:
	cp -r $(PORT_DIR)/include $@

LICENCE:
	cp $(PORT_DIR)/src/lib/icu/license.html $@

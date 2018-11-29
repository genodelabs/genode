MIRROR_FROM_REP_DIR = lib/import/import-solo5.mk lib/symbols/solo5

content: $(MIRROR_FROM_REP_DIR) include/solo5 LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/solo5)/src/lib/solo5

include/solo5:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/solo5/* $@
	cp -r $(REP_DIR)/include/solo5/* $@

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/$@ $@

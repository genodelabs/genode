content: include lib/symbols/solo5 LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/solo5)/src/lib/solo5

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include $@
	cp -r $(REP_DIR)/include $@

lib/symbols/solo5:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/$@ $@

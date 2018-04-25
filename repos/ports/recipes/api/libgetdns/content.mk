content: include lib/symbols/libgetdns LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/getdns)

include:
	cp -r $(PORT_DIR)/$@ $@

lib/symbols/libgetdns:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/getdns/$@ $@

content: include/config.h

include/config.h:
	mkdir -p $(dir $@)
	cp $(REP_DIR)/src/lib/getdns/config.h $@

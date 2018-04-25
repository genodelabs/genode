MIRROR_FROM_REP_DIR = lib/import/import-libgetdns.mk lib/mk/libgetdns.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

MIRROR_FROM_PORT_DIR = src/lib/getdns
content: $(MIRROR_FROM_PORT_DIR)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/getdns)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

content: src/lib/getdns/target.mk LICENSE

src/lib/getdns/target.mk:
	mkdir -p $(dir $@)
	echo 'LIBS=libgetdns' > $@

LICENSE:
	cp $(PORT_DIR)/src/lib/getdns/$@ $@

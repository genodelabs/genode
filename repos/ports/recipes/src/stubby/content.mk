content: src/lib/getdns/stubby LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/getdns)

STUBBY_SRC_DIR := $(PORT_DIR)/src/lib/getdns/stubby

MIRROR_FROM_PORT_DIR = src/lib/getdns/stubby include/sldns src/lib/getdns/src/gldns
content: $(MIRROR_FROM_PORT_DIR)

include/sldns:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/src/lib/getdns/src/util/auxiliary/sldns $@

src/lib/getdns/src/gldns:
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

src/lib/getdns/stubby:
	mkdir -p $(dir $@)
	cp -r $(STUBBY_SRC_DIR) $@

LICENSE:
	cp $(STUBBY_SRC_DIR)/COPYING $@

MIRROR_FROM_REP_DIR := \
	src/app/stubby \
	lib/mk/getdns-gldns.mk lib/import/import-libgetdns.mk \
	src/app/stubby/config.h \

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: include/config.h

include/config.h:
	mkdir -p $(dir $@)
	cp $(REP_DIR)/src/app/stubby/config.h $@

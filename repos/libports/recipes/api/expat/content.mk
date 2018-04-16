MIRROR_FROM_REP_DIR := lib/import/import-expat.mk \
                       lib/symbols/expat

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/expat)

content: include

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/* $@

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/expat/contrib/COPYING $@

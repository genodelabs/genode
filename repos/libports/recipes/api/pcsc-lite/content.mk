MIRROR_FROM_REP_DIR := lib/import/import-pcsc-lite.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/pcsc-lite)

content: include

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/PCSC/* $@

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/pcsc-lite/COPYING $@

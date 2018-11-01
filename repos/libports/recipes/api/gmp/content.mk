MIRROR_FROM_REP_DIR := lib/import/import-gmp.mk lib/symbols/gmp

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/gmp)

content: $(MIRROR_FROM_REP_DIR) include LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/* $@/
	cp -r $(REP_DIR)/include/gmp/* $@/
	cp -r $(REP_DIR)/include/spec/32bit/gmp/* $@/
	cp -r $(REP_DIR)/include/spec/64bit/gmp/* $@/

LICENSE:
	cp $(PORT_DIR)/src/lib/gmp/COPYING $@

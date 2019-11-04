content: include lib/symbols/mpfr LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mpfr)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/mpfr/* $@/
	cp -r $(PORT_DIR)/include/generic $@/
	cp -r $(PORT_DIR)/include/arm $@/
	cp -r $(PORT_DIR)/include/x86 $@/

lib/symbols/mpfr:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/mpfr/COPYING $@

content: include lib/symbols/mpfr LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mpfr)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/mpfr/* $@/

lib/symbols/mpfr:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/mpfr/COPYING $@

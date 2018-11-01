content: src/lib/mpfr lib/mk/mpfr.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mpfr)

src/lib/mpfr:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/mpfr/* $@
	echo "LIBS = mpfr" > $@/target.mk

lib/mk/mpfr.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/mpfr/COPYING $@

content: src/lib/jbig2dec lib/mk/jbig2dec.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/jbig2dec)

src/lib/jbig2dec:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/lib/jbig2dec/* $@/
	echo "LIBS = jbig2dec" > $@/target.mk

lib/mk/%.mk:
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/jbig2dec/LICENSE $@

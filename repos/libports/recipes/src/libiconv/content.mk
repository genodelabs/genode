MIRROR_FROM_REP_DIR := lib/mk/libiconv.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/libiconv

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libiconv)

src/lib/libiconv:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/libiconv/* $@
	cp -r $(REP_DIR)/src/lib/libiconv/private $@
	echo "LIBS = libiconv" > $@/target.mk

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/libiconv/COPYING.LIB $@


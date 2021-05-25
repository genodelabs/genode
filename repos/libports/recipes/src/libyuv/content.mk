MIRROR_FROM_REP_DIR := lib/mk/libyuv.inc  lib/mk/spec/x86_32/libyuv.mk \
                       lib/mk/spec/x86_64/libyuv.mk lib/import/import-libyuv.mk

content: src/lib/libyuv include LICENSE $(MIRROR_FROM_REP_DIR)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libyuv)

src/lib/libyuv:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/libyuv/source $@
	echo "LIBS = libyuv" > $@/target.mk

include:
	mkdir -p $@
	cp -a $(PORT_DIR)/include/* $@


$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	cp $(PORT_DIR)/src/lib/libyuv/LICENSE $@

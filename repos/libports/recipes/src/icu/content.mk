content: src/lib/icu lib/mk/icu.mk LICENCE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/icu)

src/lib/icu:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/lib/icu/* $@/
	echo "LIBS = icu" > $@/target.mk

lib/mk/icu.mk:
	$(mirror_from_rep_dir)

LICENCE:
	cp $(PORT_DIR)/src/lib/icu/license.html $@

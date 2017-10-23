content: src/lib/pcre lib/mk/pcre.mk LICENCE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/pcre)

src/lib/pcre:
	mkdir -p $@
	cp -a $(PORT_DIR)/src/lib/pcre/* $@
	cp -a $(REP_DIR)/src/lib/pcre/* $@
	echo "LIBS = pcre" > $@/target.mk

lib/mk/pcre.mk:
	$(mirror_from_rep_dir)

LICENCE:
	cp $(PORT_DIR)/src/lib/pcre/LICENCE $@


MIRROR_FROM_REP_DIR := lib/import/import-pcre.mk \
                       lib/symbols/pcre16

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: include LICENCE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/pcre)

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/pcre/* $@/

LICENCE:
	cp $(PORT_DIR)/src/lib/pcre/LICENCE $@

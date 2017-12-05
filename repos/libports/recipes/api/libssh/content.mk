MIRROR_FROM_REP_DIR := lib/symbols/libssh

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libssh)

content: include/libssh

include/libssh:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/libssh/* $@/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/libssh/COPYING $@

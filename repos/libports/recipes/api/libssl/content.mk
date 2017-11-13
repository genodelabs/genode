MIRROR_FROM_REP_DIR := lib/import/import-libssl.mk \
                       lib/symbols/libssl

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/openssl)

content: include

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/
	cp $(PORT_DIR)/include/openssl/e_os* $@/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/openssl/LICENSE $@

MIRROR_FROM_REP_DIR := \
	lib/import/import-libcrypto.mk \
	lib/import/import-libssl.mk \
	lib/import/import-openssl.mk \
	lib/symbols/libcrypto \
	lib/symbols/libssl

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/openssl)

content: include

include:
	mkdir $@
	cp -r $(PORT_DIR)/include/* $@/

content: src/lib/openssl

src/lib/openssl:
	mkdir -p $@
	cp -r $(REP_DIR)/src/lib/openssl/crypto $@/
	cp -r $(REP_DIR)/src/lib/openssl/spec $@/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/openssl/LICENSE $@


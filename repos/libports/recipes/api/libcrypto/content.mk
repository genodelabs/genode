MIRROR_FROM_REP_DIR := lib/import/import-libcrypto.mk \
                       lib/symbols/libcrypto

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/openssl)

content: include

include:
	mkdir -p $@
	cp -r $(PORT_DIR)/include/* $@/
	mkdir -p $@/crypto/ec
	cp -r $(PORT_DIR)/src/lib/openssl/crypto/ec/ec_lcl.h $@/crypto/ec
	cp -r $(PORT_DIR)/src/lib/openssl/crypto/o_dir.h $@/crypto

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/openssl/LICENSE $@

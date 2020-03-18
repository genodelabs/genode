MIRROR_FROM_REP_DIR := \
	lib/mk/libcrypto.inc \
	lib/mk/spec/arm/libcrypto.mk \
	lib/mk/spec/arm_64/libcrypto.mk \
	lib/mk/spec/x86_32/libcrypto.mk \
	lib/mk/spec/x86_64/libcrypto.mk \
	lib/mk/libssl.mk

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: src/lib/openssl/target.mk

src/lib/openssl/target.mk: src/lib/openssl
	mkdir -p $(dir $@)
	echo "LIBS += libcrypto libssl" > $@

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/openssl)

content: src/lib/openssl

src/lib/openssl:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/openssl/crypto $@/
	cp    $(PORT_DIR)/src/lib/openssl/e_os.h $@/
	cp -r $(PORT_DIR)/src/lib/openssl/include $@/
	cp -r $(PORT_DIR)/src/lib/openssl/ssl $@/
	cp -r $(REP_DIR)/src/lib/openssl/crypto $@/
	cp -r $(REP_DIR)/src/lib/openssl/spec $@/

content: LICENSE

LICENSE:
	cp $(PORT_DIR)/src/lib/openssl/LICENSE $@

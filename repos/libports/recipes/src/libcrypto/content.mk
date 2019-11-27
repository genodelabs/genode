content: src/lib/libcrypto/target.mk src/lib/openssl lib/mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/openssl)

src/lib/libcrypto:
	mkdir -p $@

src/lib/openssl:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/openssl/e_os.h $@
	cp -r $(REP_DIR)/src/lib/openssl/* $@
	mkdir -p $@/crypto
	cp -r $(PORT_DIR)/src/lib/openssl/crypto/* $@/crypto

src/lib/libcrypto/target.mk: src/lib/libcrypto src/lib/openssl
	echo "LIBS += libcrypto" > $@

lib/mk:
	mkdir -p $@
	cp $(REP_DIR)/lib/mk/libcrypto.inc $@
	for spec in x86_32 x86_64 arm arm_64; do \
	  mkdir -p $@/spec/$$spec; \
	  cp $(REP_DIR)/$@/spec/$$spec/libcrypto.mk $@/spec/$$spec/; \
	done

LICENSE:
	cp $(PORT_DIR)/src/lib/openssl/LICENSE $@

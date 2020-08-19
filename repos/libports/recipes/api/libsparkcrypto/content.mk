LSC_DIR := \
	$(call port_dir,$(GENODE_DIR)/repos/libports/ports/libsparkcrypto)
LSC_ALI_DIR := \
	$(call port_dir,$(GENODE_DIR)/repos/libports/ports/libsparkcrypto)/libsparkcrypto-alis/lib/ali/libsparkcrypto

MIRROR_FROM_LSC_DIR := \
	$(addprefix libsparkcrypto/src/shared/generic/,\
		lsc-aes-cbc.ads \
		lsc-aes.ads \
		lsc-aes_generic.adb \
		lsc-aes_generic-cbc.adb \
		lsc-aes_generic-cbc.ads \
		lsc-aes_generic.ads \
		lsc-internal-aes-cbc.ads \
		lsc-internal-aes-print.ads \
		lsc-internal-aes-tables.ads \
		lsc-internal-aes.ads \
		lsc-internal-bignum.ads \
		lsc-internal-byteorder32.ads \
		lsc-internal-byteorder64.ads \
		lsc-internal-byteswap32.ads \
		lsc-internal-byteswap64.ads \
		lsc-internal-convert.ads \
		lsc-internal-convert_hash.adb \
		lsc-internal-convert_hash.ads \
		lsc-internal-convert_hmac.ads \
		lsc-internal-debug.ads \
		lsc-internal-ec.ads \
		lsc-internal-ec_signature.ads \
		lsc-internal-hmac_ripemd160.ads \
		lsc-internal-hmac_sha1.ads \
		lsc-internal-hmac_sha256.ads \
		lsc-internal-hmac_sha384.ads \
		lsc-internal-hmac_sha512.ads \
		lsc-internal-io.ads \
		lsc-internal-math_int.ads \
		lsc-internal-ops32.ads \
		lsc-internal-ops64.ads \
		lsc-internal-pad32.ads \
		lsc-internal-pad64.ads \
		lsc-internal-ripemd160-print.ads \
		lsc-internal-ripemd160.ads \
		lsc-internal-sha1.ads \
		lsc-internal-sha256-tables.ads \
		lsc-internal-sha256.ads \
		lsc-internal-sha512-tables.ads \
		lsc-internal-sha512.ads \
		lsc-internal-types.ads \
		lsc-internal.ads \
		lsc-ops_generic.adb \
		lsc-ops_generic.ads \
		lsc-ripemd160-hmac.ads \
		lsc-ripemd160-hmac_generic.ads \
		lsc-ripemd160.ads \
		lsc-ripemd160_generic.ads \
		lsc-sha1-hmac.ads \
		lsc-sha1-hmac_generic.ads \
		lsc-sha1.ads \
		lsc-sha1_generic.adb \
		lsc-sha1_generic.ads \
		lsc-sha2-hmac.ads \
		lsc-sha2-hmac_generic.ads \
		lsc-sha2.ads \
		lsc-sha2_generic.adb \
		lsc-sha2_generic.ads \
		lsc-types.ads \
		lsc.ads \
	) \
	$(addprefix libsparkcrypto/src/ada/generic/,\
		lsc-internal-debug.ads \
	) \
	$(addprefix libsparkcrypto/src/ada/debug/,\
		lsc-internal-aes-print.ads \
		lsc-internal-bignum-print.ads \
		lsc-internal-ripemd160-print.ads \
	)

MIRROR_FROM_LSC_ALI_DIR := \
	lsc-aes-cbc.ali \
	lsc-aes.ali \
	lsc-aes_generic-cbc.ali \
	lsc-aes_generic.ali \
	lsc-internal-aes-cbc.ali \
	lsc-internal-aes-print.ali \
	lsc-internal-aes-tables.ali \
	lsc-internal-aes.ali \
	lsc-internal-bignum.ali \
	lsc-internal-byteorder32.ali \
	lsc-internal-byteorder64.ali \
	lsc-internal-byteswap32.ali \
	lsc-internal-byteswap64.ali \
	lsc-internal-convert.ali \
	lsc-internal-convert_hash.ali \
	lsc-internal-convert_hmac.ali \
	lsc-internal-debug.ali \
	lsc-internal-ec.ali \
	lsc-internal-ec_signature.ali \
	lsc-internal-hmac_ripemd160.ali \
	lsc-internal-hmac_sha1.ali \
	lsc-internal-hmac_sha256.ali \
	lsc-internal-hmac_sha384.ali \
	lsc-internal-hmac_sha512.ali \
	lsc-internal-io.ali \
	lsc-internal-math_int.ali \
	lsc-internal-ops32.ali \
	lsc-internal-ops64.ali \
	lsc-internal-pad32.ali \
	lsc-internal-pad64.ali \
	lsc-internal-ripemd160-print.ali \
	lsc-internal-ripemd160.ali \
	lsc-internal-sha1.ali \
	lsc-internal-sha256-tables.ali \
	lsc-internal-sha256.ali \
	lsc-internal-sha512-tables.ali \
	lsc-internal-sha512.ali \
	lsc-internal-types.ali \
	lsc-internal.ali \
	lsc-ops_generic.ali \
	lsc-ripemd160-hmac.ali \
	lsc-ripemd160-hmac_generic.ali \
	lsc-ripemd160.ali \
	lsc-ripemd160_generic.ali \
	lsc-sha1-hmac.ali \
	lsc-sha1-hmac_generic.ali \
	lsc-sha1.ali \
	lsc-sha1_generic.ali \
	lsc-sha2-hmac.ali \
	lsc-sha2-hmac_generic.ali \
	lsc-sha2.ali \
	lsc-sha2_generic.ali \
	lsc-types.ali \
	lsc.ali

content: $(MIRROR_FROM_LSC_DIR) \
         $(MIRROR_FROM_LSC_ALI_DIR)

$(MIRROR_FROM_LSC_DIR):
	mkdir -p include
	cp -a $(LSC_DIR)/$@ include/

$(MIRROR_FROM_LSC_ALI_DIR):
	mkdir -p lib/ali/libsparkcrypto
	cp -a $(LSC_ALI_DIR)/$@ lib/ali/libsparkcrypto/

MIRROR_FROM_REP_DIR := \
	lib/import/import-libsparkcrypto.mk \
	lib/symbols/libsparkcrypto \

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: LICENSE

LICENSE:
	echo "BSD-3-Clause-Attribution, see libsparkcrypto/README.rst" > $@

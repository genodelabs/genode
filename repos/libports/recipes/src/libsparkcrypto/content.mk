LSC_DIR := \
	$(call port_dir,$(GENODE_DIR)/repos/libports/ports/libsparkcrypto)

MIRROR_FROM_LSC_DIR := \
	$(addprefix libsparkcrypto/src/shared/generic/,\
		lsc-types.ads \
		lsc-internal-aes-tables.ads \
		lsc-internal-ripemd160-print.ads \
		lsc-internal-aes-print.ads \
		lsc-internal-sha512-tables.ads \
		lsc-internal-sha256-tables.ads \
		lsc-internal-math_int.ads \
		lsc-internal-convert.ads \
		lsc-internal.ads \
		lsc-internal-debug.ads \
		lsc.ads \
		lsc-aes_generic.adb \
		lsc-aes_generic-cbc.adb \
		lsc-internal-convert_hash.adb \
		lsc-internal-convert_hmac.adb \
		lsc-ops_generic.adb \
		lsc-ripemd160_generic.adb \
		lsc-ripemd160-hmac_generic.adb \
		lsc-sha1_generic.adb \
		lsc-sha1-hmac_generic.adb \
		lsc-sha2_generic.adb \
		lsc-sha2-hmac_generic.adb \
		lsc-internal-aes.adb \
		lsc-internal-aes-cbc.adb \
		lsc-internal-bignum.adb \
		lsc-internal-byteswap32.adb \
		lsc-internal-byteswap64.adb \
		lsc-internal-ec.adb \
		lsc-internal-ec_signature.adb \
		lsc-internal-hmac_ripemd160.adb \
		lsc-internal-hmac_sha1.adb \
		lsc-internal-hmac_sha256.adb \
		lsc-internal-hmac_sha384.adb \
		lsc-internal-hmac_sha512.adb \
		lsc-internal-ops32.adb \
		lsc-internal-ops64.adb \
		lsc-internal-pad32.adb \
		lsc-internal-pad64.adb \
		lsc-internal-ripemd160.adb \
		lsc-internal-sha1.adb \
		lsc-internal-sha256.adb \
		lsc-internal-sha512.adb) \
	$(addprefix libsparkcrypto/src/ada/nullio/,\
		lsc-internal-io.adb) \
	$(addprefix libsparkcrypto/src/shared/little_endian/,\
		lsc-internal-byteorder32.adb \
		lsc-internal-byteorder64.adb) \
	$(addprefix libsparkcrypto/src/ada/generic/,\
		lsc-internal-debug.ads \
		lsc-internal-types.adb) \
	$(addprefix libsparkcrypto/src/ada/debug/,\
		lsc-internal-ripemd160-print.ads \
		lsc-internal-aes-print.ads) \
	$(addprefix libsparkcrypto/build/,\
		pragmas.adc)

content: $(MIRROR_FROM_LSC_DIR)

$(MIRROR_FROM_LSC_DIR):
	mkdir -p $(dir $@)
	cp -a $(LSC_DIR)/$@ $(dir $@)

MIRROR_FROM_REP_DIR := \
	lib/mk/libsparkcrypto.mk \
	lib/mk/libsparkcrypto.inc \
	lib/import/import-libsparkcrypto.mk \

content: $(MIRROR_FROM_REP_DIR)

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

content: LICENSE

LICENSE:
	echo "BSD-3-Clause-Attribution, see libsparkcrypto/README.rst" > $@

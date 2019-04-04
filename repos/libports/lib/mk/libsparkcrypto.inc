include $(call select_from_repositories,lib/import/import-libsparkcrypto.mk)

SRC_ADS = \
	lsc.ads \
	lsc-internal-debug.ads \
	lsc-internal.ads \
	lsc-internal-aes-tables.ads \
	lsc-internal-convert.ads \
	lsc-internal-debug.ads \
	lsc-internal-math_int.ads \
	lsc-internal-sha256-tables.ads \
	lsc-internal-sha512-tables.ads \
	lsc-types.ads \
	lsc-internal-aes-print.ads \
	lsc-internal-ripemd160-print.ads \

SRC_ADB = \
	lsc-internal-types.adb \
	lsc-internal-io.adb \
	lsc-internal-aes.adb \
	lsc-internal-aes-cbc.adb \
	lsc-internal-bignum.adb \
	lsc-internal-byteorder32.adb \
	lsc-internal-byteorder64.adb \
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
	lsc-internal-sha512.adb \

SRC_ADB += \
	lsc-aes.adb \
	lsc-aes-cbc.adb \
	lsc-aes_generic.adb \
	lsc-aes_generic-cbc.adb \
	lsc-internal-convert_hash.adb \
	lsc-internal-convert_hmac.adb \
	lsc-ops_generic.adb \
	lsc-ripemd160.adb \
	lsc-ripemd160_generic.adb \
	lsc-ripemd160-hmac.adb \
	lsc-ripemd160-hmac_generic.adb \
	lsc-sha1.adb \
	lsc-sha1_generic.adb \
	lsc-sha1-hmac.adb \
	lsc-sha1-hmac_generic.adb \
	lsc-sha2.adb \
	lsc-sha2_generic.adb \
	lsc-sha2-hmac.adb \
	lsc-sha2-hmac_generic.adb \


# Normally, we use the minimal 'spark' runtime. The unit tests, however, use the
# fat 'ada' runtime and we must not mix the two. Hence, make the runtime
# configurable from outside.
LSC_RUNTIME ?= spark

LIBS += $(LSC_RUNTIME)

vpath lsc-internal-io.adb $(LSC_DIR)/ada/nullio
vpath lsc-internal-byteorder32.adb $(LSC_DIR)/shared/little_endian
vpath lsc-internal-byteorder64.adb $(LSC_DIR)/shared/little_endian
vpath lsc-internal-types.adb $(LSC_DIR)/ada/generic
vpath %.adb $(LSC_DIR)/shared/generic
vpath %.ads $(LSC_DIR)/shared/generic

SHARED_LIB = yes
CC_ADA_OPT += -gnatec=$(LSC_DIR)/../build/pragmas.adc -fPIC

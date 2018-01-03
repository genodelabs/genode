SRC_C += $(notdir $(wildcard $(GMP_DIR)/mpz/*.c))

include $(REP_DIR)/lib/mk/gmp.inc

vpath %.c $(GMP_DIR)/mpz

CC_CXX_WARN_STRICT =

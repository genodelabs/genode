SRC_C += $(notdir $(wildcard $(GMP_DIR)/mpq/*.c))

include $(REP_DIR)/lib/mk/gmp.inc

vpath %.c $(GMP_DIR)/mpq

CC_CXX_WARN_STRICT =

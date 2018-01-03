SRC_C += $(notdir $(wildcard $(GMP_DIR)/mpf/*.c))

include $(REP_DIR)/lib/mk/gmp.inc

vpath %.c $(GMP_DIR)/mpf

CC_CXX_WARN_STRICT =

GMP_DIR = $(call select_from_ports,gmp)/src/lib/gmp

include $(REP_DIR)/lib/import/import-gmp.mk

LIBS += libc

CC_OPT += -DHAVE_CONFIG_H -D__GMP_WITHIN_GMP

INC_DIR += $(GMP_DIR)

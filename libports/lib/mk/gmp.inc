GMP_DIR = $(REP_DIR)/contrib/gmp-4.3.2

ifeq ($(wildcard $(GMP_DIR)),)
REQUIRES += prepare_gmp
endif

include $(REP_DIR)/lib/import/import-gmp.mk

LIBS += libc

CC_OPT  += -DHAVE_CONFIG_H -D__GMP_WITHIN_GMP

INC_DIR += $(REP_DIR)/include/gcc

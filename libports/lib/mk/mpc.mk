include $(REP_DIR)/lib/import/import-mpc.mk

MPC_DIR  = $(REP_DIR)/contrib/$(MPC)

ifeq ($(wildcard $(MPC_DIR)),)
REQUIRES += prepare_mpc
endif

LIBS      = libc gmp mpfr

SRC_C := $(notdir $(wildcard $(MPC_DIR)/src/*.c))

vpath %.c $(MPC_DIR)/src

SHARED_LIB = 1

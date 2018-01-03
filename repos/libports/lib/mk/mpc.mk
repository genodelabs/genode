include $(REP_DIR)/lib/import/import-mpc.mk

MPC_DIR := $(call select_from_ports,mpc)/src/lib/mpc

LIBS := libc gmp mpfr

SRC_C := $(notdir $(wildcard $(MPC_DIR)/src/*.c))

vpath %.c $(MPC_DIR)/src

SHARED_LIB = 1

CC_CXX_WARN_STRICT =

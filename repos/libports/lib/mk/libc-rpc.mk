LIBC_RPC_DIR = $(LIBC_DIR)/lib/libc/rpc

#SRC_C = $(notdir $(wildcard $(LIBC_RPC_DIR)/*.c))

SRC_C = bindresvport.c

include $(REP_DIR)/lib/mk/libc-common.inc

INC_DIR += $(LIBC_REP_DIR)/include/libc/sys
INC_DIR += $(LIBC_PORT_DIR)/include/libc/sys

vpath %.c $(LIBC_RPC_DIR)

SRC_C    = bootinfo.c
LIBS    += syscall-okl4
INC_DIR += $(REP_DIR)/src/include/bootinfo/internal
CC_WARN  = -Wall -Wno-attributes
OKL4_DIR = $(call select_from_ports,okl4)/src/kernel/okl4

vpath bootinfo.c $(OKL4_DIR)/libs/bootinfo/src

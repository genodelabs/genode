SRC_C    = bootinfo.c
INC_DIR += $(REP_DIR)/src/include/bootinfo/internal
CC_WARN  = -Wall -Wno-attributes

vpath bootinfo.c $(OKL4_DIR)/libs/bootinfo/src

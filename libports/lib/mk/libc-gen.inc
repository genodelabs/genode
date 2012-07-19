LIBC_GEN_DIR = $(LIBC_DIR)/libc/gen

# this file produces a warning about a missing header file, lets drop it
FILTER_OUT_C += getosreldate.c sem.c valloc.c getpwent.c

SRC_C = $(filter-out $(FILTER_OUT_C),$(notdir $(wildcard $(LIBC_GEN_DIR)/*.c)))

# 'sysconf.c' includes the local 'stdtime/tzfile.h'
INC_DIR += $(REP_DIR)/src/lib/libc/stdtime

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_GEN_DIR)

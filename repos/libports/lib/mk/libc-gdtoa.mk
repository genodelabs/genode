GDTOA_DIR      = $(LIBC_DIR)/contrib/gdtoa
LIBC_GDTOA_DIR = $(LIBC_DIR)/lib/libc/gdtoa

FILTER_OUT  = arithchk.c strtodnrp.c qnan.c
FILTER_OUT += machdep_ldisQ.c machdep_ldisx.c

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(GDTOA_DIR)/*.c))) \
        $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIBC_GDTOA_DIR)/*.c)))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(GDTOA_DIR)
vpath %.c $(LIBC_GDTOA_DIR)

CC_CXX_WARN_STRICT =

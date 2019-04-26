LIBC_STDTIME_DIR = $(LIBC_DIR)/lib/libc/stdtime

INC_DIR += $(LIBC_DIR)/contrib/tzcode/stdtime

SRC_C = $(notdir $(wildcard $(LIBC_STDTIME_DIR)/*.c))

CC_OPT_localtime = -DTZ_MAX_TIMES=1

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_STDTIME_DIR)

CC_CXX_WARN_STRICT =

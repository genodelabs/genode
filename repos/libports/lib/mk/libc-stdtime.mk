LIBC_STDTIME_DIR = $(LIBC_DIR)/libc/stdtime

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIBC_STDTIME_DIR)/*.c)))

CC_OPT_localtime = -DTZ_MAX_TIMES=1

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_STDTIME_DIR)

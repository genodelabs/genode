TZCODE_DIR      = $(LIBC_DIR)/contrib/tzcode/stdtime

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(TZCODE_DIR)/*.c)))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(TZCODE_DIR)

CC_CXX_WARN_STRICT =

LIBC_COMPAT_DIR = $(LIBC_DIR)/lib/libc/compat-43

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIBC_COMPAT_DIR)/*.c)))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_COMPAT_DIR)

CC_CXX_WARN_STRICT =

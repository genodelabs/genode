LIBC_INET_DIR = $(LIBC_DIR)/lib/libc/inet

SRC_C = $(filter-out $(FILTER_OUT_C),$(notdir $(wildcard $(LIBC_INET_DIR)/*.c)))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_INET_DIR)

CC_CXX_WARN_STRICT =

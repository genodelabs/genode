LIBC_REGEX_DIR = $(LIBC_DIR)/lib/libc/regex

#
# 'engine.c' is meant to be included by other compilation units. It cannot
# be compiled individually.
#
FILTER_OUT = engine.c

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIBC_REGEX_DIR)/*.c)))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_REGEX_DIR)

CC_CXX_WARN_STRICT =

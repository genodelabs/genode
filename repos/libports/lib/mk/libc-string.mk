#
# Portion of the string library that is used by both the freestanding string
# library and the complete libc
#

#
# These files would infect the freestanding string library with the locale
# library
#
FILTER_OUT = strcoll.c strxfrm.c wcscoll.c wcsxfrm.c

LIBC_STRING_DIR = $(LIBC_DIR)/lib/libc/string

SRC_C = $(filter-out $(FILTER_OUT),$(notdir $(wildcard $(LIBC_STRING_DIR)/*.c)))

include $(REP_DIR)/lib/mk/libc-common.inc

vpath %.c $(LIBC_STRING_DIR)

CC_CXX_WARN_STRICT =

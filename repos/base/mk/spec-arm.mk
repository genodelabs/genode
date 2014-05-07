#
# ARM-specific Genode headers
#
REP_INC_DIR += include/arm

SPECS += 32bit

#
# Prevent compiler message
# "note: the mangling of 'va_list' has changed in GCC 4.4"
#
CC_OPT += -Wno-psabi

include $(call select_from_repositories,mk/spec-32bit.mk)

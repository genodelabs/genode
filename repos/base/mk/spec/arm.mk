#
# ARM-specific Genode headers
#
REP_INC_DIR += include/spec/arm

SPECS += 32bit

NIM_CPU ?= arm

#
# Prevent compiler message
# "note: the mangling of 'va_list' has changed in GCC 4.4"
#
CC_C_OPT   += -Wno-psabi
CC_CXX_OPT += -Wno-psabi

include $(BASE_DIR)/mk/spec/32bit.mk

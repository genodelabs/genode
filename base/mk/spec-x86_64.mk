#
# Specifics for 64-bit x86
#
SPECS += x86 64bit

#
# x86-specific Genode headers
#
REP_INC_DIR += include/x86
REP_INC_DIR += include/x86_64

CC_MARCH ?= -m64

include $(call select_from_repositories,mk/spec-64bit.mk)

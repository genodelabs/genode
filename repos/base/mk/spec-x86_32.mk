#
# Specifics for 32-bit x86
#
SPECS += x86 32bit

#
# x86-specific Genode headers
#
REP_INC_DIR += include/x86
REP_INC_DIR += include/x86_32
REP_INC_DIR += include/platform/x86

#
# x86-specific flags
#
CC_MARCH ?= -march=i686 -m32
LD_MARCH ?= -melf_i386
AS_MARCH ?= -march=i686 --32

include $(call select_from_repositories,mk/spec-32bit.mk)

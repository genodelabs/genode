#
# Specifics for 32-bit x86
#
SPECS += x86 32bit

#
# x86-specific Genode headers
#
REP_INC_DIR += include/spec/x86
REP_INC_DIR += include/spec/x86_32

#
# x86-specific flags
#
CC_MARCH ?= -march=i686 -m32
LD_MARCH ?= -melf_i386
AS_MARCH ?= -march=i686 --32

NIM_CPU ?= i386

include $(BASE_DIR)/mk/spec/32bit.mk
include $(BASE_DIR)/mk/spec/x86.mk

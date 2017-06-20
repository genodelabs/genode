#
# Specifics for 64-bit x86
#
SPECS += x86 64bit

#
# x86-specific Genode headers
#
REP_INC_DIR += include/spec/x86
REP_INC_DIR += include/spec/x86_64

CC_MARCH ?= -m64 -mcmodel=large

#
# Avoid wasting almost 4 MiB by telling the linker that the max page size is
# 4K. Otherwise, the linker would align the text segment to a 4M boundary,
# effectively adding 4M of zeros to each binary.
#
# See http://sourceware.org/ml/binutils/2009-04/msg00099.html
#
LD_MARCH ?= -melf_x86_64

NIM_CPU ?= amd64

include $(BASE_DIR)/mk/spec/64bit.mk
include $(BASE_DIR)/mk/spec/x86.mk

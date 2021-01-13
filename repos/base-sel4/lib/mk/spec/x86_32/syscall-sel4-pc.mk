PLAT := pc99
ARCH := x86

SEL4_ARCH := ia32
PLAT_BOARD := /$(SEL4_ARCH)
SEL4_WORDBITS := 32

ARCH_INCLUDES := exIPC.h vmenter.h

include $(REP_DIR)/lib/mk/syscall-sel4.inc

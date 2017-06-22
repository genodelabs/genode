PLAT := pc99
ARCH := x86

SEL4_ARCH := x86_64
PLAT_BOARD := /$(SEL4_ARCH)
SEL4_WORDBITS := 64

ARCH_INCLUDES := exIPC.h
SEL4_ARCH_INCLUDES := syscalls_syscall.h

include $(REP_DIR)/lib/mk/syscall-sel4.inc

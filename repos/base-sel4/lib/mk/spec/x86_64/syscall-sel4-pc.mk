PLAT := pc99
ARCH := x86
BOARD := pc

SEL4_ARCH := x86_64
SEL4_WORDBITS := 64

ARCH_INCLUDES := exIPC.h vmenter.h
SEL4_ARCH_INCLUDES := syscalls_syscall.h

LIBS += kernel-sel4-pc

include $(REP_DIR)/lib/mk/syscall-sel4.inc

L4_CONFIG := $(call select_from_repositories,config/x86_32.user)

L4_INC_TARGETS := x86/l4/sys x86/l4f/l4/sys x86/l4/vcpu

CC_OPT += -Iinclude/x86

L4_BIN_DIR := $(LIB_CACHE_DIR)/syscall-foc/pc-build/bin/x86_586

include $(REP_DIR)/lib/mk/spec/x86/syscall-foc.inc

vpath syscalls_direct.S $(L4_PKG_DIR)/l4sys/lib/src/ARCH-x86

SRC_C += utcb.c
SRC_S += syscalls_direct.S

include $(REP_DIR)/lib/mk/syscall-foc.inc

utcb.c:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/source/pkg/l4sys/lib/src/utcb.c

syscalls_direct.S:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/source/pkg/l4sys/lib/src/ARCH-x86/syscalls_direct.S

utcb.c syscalls_direct.S: $(PKG_TAGS)

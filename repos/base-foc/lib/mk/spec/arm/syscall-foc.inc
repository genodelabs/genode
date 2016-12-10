L4_INC_TARGETS = arm/l4/sys \
                 arm/l4f/l4/sys \
                 arm/l4/vcpu

CC_OPT += -Iinclude/arm

include $(REP_DIR)/lib/mk/syscall-foc.inc

SRC_C += utcb.c
SRC_S += atomic_ops_s.S

utcb.c:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/source/pkg/l4sys/lib/src/utcb.c

atomic_ops_s.S:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/source/pkg/l4sys/lib/src/ARCH-arm/atomic_ops_s.S

utcb.c atomic_ops_s.S: $(PKG_TAGS)

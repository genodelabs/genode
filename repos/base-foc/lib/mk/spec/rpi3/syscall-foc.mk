L4_CONFIG := $(call select_from_repositories,config/rpi3.user)

L4_BIN_DIR := $(LIB_CACHE_DIR)/syscall-foc/rpi3-build/bin/arm64_armv8a

L4_INC_TARGETS = arm64/l4/sys \
                 arm64/l4f/l4/sys \
                 arm64/l4/vcpu

CC_OPT += -Iinclude/arm64

include $(REP_DIR)/lib/mk/syscall-foc.inc

SRC_C += utcb.c

utcb.c:
	$(VERBOSE)ln -sf $(L4_BUILD_DIR)/source/pkg/l4re-core/l4sys/lib/src/utcb.c

utcb.c: $(PKG_TAGS)

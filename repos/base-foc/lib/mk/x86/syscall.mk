SRC_C += utcb.c
SRC_S += syscalls_direct.S

vpath syscalls_direct.S $(L4_BUILD_DIR)/source/pkg/l4sys/lib/src/ARCH-x86
vpath utcb.c            $(L4_BUILD_DIR)/source/pkg/l4sys/lib/src

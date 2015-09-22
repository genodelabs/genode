SRC_C += utcb.c
SRC_S += atomic_ops_s.S

vpath atomic_ops_s.S $(L4_BUILD_DIR)/source/pkg/l4sys/lib/src/ARCH-arm
vpath utcb.c         $(L4_BUILD_DIR)/source/pkg/l4sys/lib/src

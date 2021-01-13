override BOARD := pbxa9
KERNEL_CONFIG  := $(REP_DIR)/config/$(BOARD).kernel

include $(REP_DIR)/lib/mk/kernel-foc.inc

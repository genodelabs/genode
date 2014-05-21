REQUIRES       = platform_arndale
FIASCO_DIR    := $(call select_from_ports,foc)/src/kernel/foc/kernel/fiasco
KERNEL_CONFIG  = $(REP_DIR)/config/arndale.kernel

-include $(PRG_DIR)/../target.inc

REQUIRES       = platform_odroid_x2
FIASCO_DIR    := $(call select_from_ports,foc)/src/kernel/foc/kernel/fiasco
KERNEL_CONFIG  = $(REP_DIR)/config/odroid_x2.kernel

-include $(PRG_DIR)/../target.inc

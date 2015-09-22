REQUIRES       = rpi
FIASCO_DIR    := $(call select_from_ports,foc)/src/kernel/foc/kernel/fiasco
KERNEL_CONFIG  = $(REP_DIR)/config/rpi.kernel

-include $(PRG_DIR)/../../target.inc

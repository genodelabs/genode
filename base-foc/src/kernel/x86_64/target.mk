REQUIRES      = x86 64bit
FIASCO_DIR    = $(REP_DIR)/contrib/kernel/fiasco
KERNEL_CONFIG = $(FIASCO_DIR)/src/templates/globalconfig.out.amd64-mp

-include $(PRG_DIR)/../target.inc

TARGET   = ahci_drv
REQUIRES = x86

INC_DIR += $(REP_DIR)/src/drivers/ahci/spec/x86

include $(REP_DIR)/src/drivers/ahci/target.inc


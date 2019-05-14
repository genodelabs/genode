TARGET   = usb_armory_tz_vmm
INC_DIR += $(REP_DIR)/src/server/tz_vmm/spec/usb_armory
SRC_CC  += spec/usb_armory/vm.cc

include $(REP_DIR)/src/server/tz_vmm/spec/imx53/target.inc

TARGET   = usb_drv
REQUIRES = x86_64

INC_DIR += $(LIB_INC_DIR)/spec/x86_64 $(LIB_INC_DIR)/spec/x86
INC_DIR += $(REP_DIR)/src/include/spec/x86_64

include $(REP_DIR)/src/drivers/usb/spec/x86/target.inc

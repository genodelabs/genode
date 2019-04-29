include $(REP_DIR)/src/drivers/usb/target.inc

SRC_CC  += platform_device.cc platform_generic.cc

INC_DIR += $(LIB_INC_DIR)/spec/arm
INC_DIR += $(REP_DIR)/src/include/spec/arm

vpath platform_device.cc  $(LIB_DIR)/spec/arm
vpath platform_generic.cc $(LIB_DIR)/spec/arm

# vi:set ft=make :

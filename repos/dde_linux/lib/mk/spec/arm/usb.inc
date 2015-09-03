include $(REP_DIR)/lib/mk/usb.inc

SRC_CC  += platform_device.c
INC_DIR += $(LIB_INC_DIR)/spec/arm

vpath platform_device.cc $(LIB_DIR)/spec/arm

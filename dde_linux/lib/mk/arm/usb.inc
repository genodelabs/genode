SRC_C   += platform_device.c
SRC_CC  += mem.cc
INC_DIR += $(LIB_INC_DIR)/arm

vpath platform_device.c $(LIB_DIR)/arm
vpath mem.cc $(LIB_DIR)/arm

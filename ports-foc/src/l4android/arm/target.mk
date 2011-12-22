REQUIRES        = foc arm
INC_DIR        += $(REP_DIR)/include/32-bit
SRC_L4LX_CONFIG = $(REP_DIR)/config/android_config.arm
LX_TARGET       = l4android
L4LX_L4ARCH     = arm

-include $(REP_DIR)/mk/l4lx.mk

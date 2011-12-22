REQUIRES        = x86 32bit
INC_DIR        += $(REP_DIR)/include/32-bit
LX_TARGET       = l4android
SRC_L4LX_CONFIG = $(REP_DIR)/config/android_config.x86_32

-include $(REP_DIR)/mk/l4lx.mk

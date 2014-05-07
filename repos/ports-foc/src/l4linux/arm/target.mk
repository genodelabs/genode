REQUIRES        = foc arm
INC_DIR        += $(REP_DIR)/include/32-bit
SRC_L4LX_CONFIG = $(REP_DIR)/config/linux_config.arm
LX_TARGET       = l4linux
L4LX_L4ARCH     = arm

-include $(REP_DIR)/mk/l4lx.mk

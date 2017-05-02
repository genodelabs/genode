#
# \brief  Build-system configurations for Raspberry Pi
# \author Norman Feske
# \date   2013-04-05
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v6 usb framebuffer gpio

# add repository relative include paths
REP_INC_DIR += include/spec/rpi

# include implied specs
include $(BASE_DIR)/mk/spec/arm_v6.mk

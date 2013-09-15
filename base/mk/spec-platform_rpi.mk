#
# \brief  Build-system configurations for Raspberry Pi
# \author Norman Feske
# \date   2013-04-05
#

# denote wich specs are also fullfilled by this spec
SPECS += arm_v6 pl011 usb framebuffer

# add repository relative include paths
REP_INC_DIR += include/platform/rpi

# include implied specs
include $(call select_from_repositories,mk/spec-arm_v6.mk)
include $(call select_from_repositories,mk/spec-pl011.mk)

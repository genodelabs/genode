#
# Enable peripherals of the platform
#
SPECS += cortex_a9 platform_zynq cadence_gem
# SPEC += gpio framebuffer usb

#
# Pull in CPU specifics
#
SPECS += arm_v7a

#
# Add device parameters to include search path
#
REP_INC_DIR += include/platform/zynq

include $(call select_from_repositories,mk/spec-cortex_a9.mk)
include $(call select_from_repositories,mk/spec-arm_v7a.mk)

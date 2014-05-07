REP_INC_DIR += include/platform/vpb926

#
# Enable peripherals of the platform
#
SPECS += pl050 pl11x pl011 ps2 framebuffer

#
# Pull in CPU specifics
#
SPECS += arm_v5

#
# Add device parameters to include search path
#
REP_INC_DIR += include/platform/vpb926

include $(call select_from_repositories,mk/spec-arm_v5.mk)

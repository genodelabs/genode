#
# Enable peripherals of the platform
#
SPECS += pl050 pl11x ps2 pl180 lan9118 pl011

#
# Pull in CPU specifics
#
SPECS += arm_v7a

#
# Add device parameters to include search path
#
REP_INC_DIR += include/platform/pbxa9

include $(call select_from_repositories,mk/spec-arm_v7a.mk)

#
# Enable peripherals of the platform
#
SPECS += pl050 pl11x ps2 pl180 lan9118 pl011

#
# Pull in CPU specifics
#
SPECS += cortex_a9

#
# Add device parameters to include search path
#
REP_INC_DIR += include/platform/vea9x4

include $(call select_from_repositories,mk/spec-cortex_a9.mk)
include $(call select_from_repositories,mk/spec-pl011.mk)

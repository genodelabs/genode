SPECS += arm

#
# Configure target CPU
#
CC_OPT += -march=armv5

include $(call select_from_repositories,mk/spec-arm.mk)

SPECS += codezero_arm_v5 platform_vpb926

CC_OPT += -D__PLATFORM__=pb926

include $(call select_from_repositories,mk/spec-codezero_arm_v5.mk)
include $(call select_from_repositories,mk/spec-platform_vpb926.mk)

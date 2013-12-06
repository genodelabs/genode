SPECS += foc_arm platform_arndale

include $(call select_from_repositories,mk/spec-fpu_vfpv3.mk)
include $(call select_from_repositories,mk/spec-platform_arndale.mk)
include $(call select_from_repositories,mk/spec-foc_arm.mk)

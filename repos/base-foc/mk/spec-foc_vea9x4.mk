SPECS += foc_arm platform_vea9x4

include $(call select_from_repositories,mk/spec-platform_vea9x4.mk)
include $(call select_from_repositories,mk/spec-foc_arm.mk)

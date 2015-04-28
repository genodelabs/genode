SPECS += foc_arm platform_odroid-x2

include $(call select_from_repositories,mk/spec-platform_odroid-x2.mk)
include $(call select_from_repositories,mk/spec-foc_arm.mk)

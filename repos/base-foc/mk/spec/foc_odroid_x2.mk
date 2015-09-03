SPECS += foc_arm odroid_x2

include $(call select_from_repositories,mk/spec/odroid_x2.mk)
include $(call select_from_repositories,mk/spec/foc_arm.mk)

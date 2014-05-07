SPECS += foc_arm platform_pbxa9

include $(call select_from_repositories,mk/spec-platform_pbxa9.mk)
include $(call select_from_repositories,mk/spec-foc_arm.mk)

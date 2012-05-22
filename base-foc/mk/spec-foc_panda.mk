SPECS += foc_arm platform_panda

include $(call select_from_repositories,mk/spec-platform_panda.mk)
include $(call select_from_repositories,mk/spec-foc_arm.mk)

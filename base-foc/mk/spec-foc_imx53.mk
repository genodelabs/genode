SPECS += foc_arm platform_imx53

include $(call select_from_repositories,mk/spec-platform_imx53.mk)
include $(call select_from_repositories,mk/spec-foc_arm.mk)

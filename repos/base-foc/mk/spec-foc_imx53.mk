SPECS += foc_arm platform_imx53

REP_INC_DIR += include/platform/imx53_qsb

include $(call select_from_repositories,mk/spec-platform_imx53.mk)
include $(call select_from_repositories,mk/spec-foc_arm.mk)

SPECS += foc_arm imx53 imx53_qsb

# add repository relative include paths
REP_INC_DIR += include/spec/imx53_qsb

include $(call select_from_repositories,mk/spec/imx53.mk)
include $(call select_from_repositories,mk/spec/foc_arm.mk)

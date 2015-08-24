SPECS += foc_arm platform_rpi

REP_INC_DIR += include/platform/rpi

include $(call select_from_repositories,mk/spec-platform_rpi.mk)
include $(call select_from_repositories,mk/spec-foc_arm.mk)

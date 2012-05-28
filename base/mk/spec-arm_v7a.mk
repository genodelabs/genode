SPECS += arm

#
# Configure target CPU
#
CC_MARCH += -march=armv7-a

REP_INC_DIR += include/arm_v7a

include $(call select_from_repositories,mk/spec-arm.mk)

SPECS += arm

# add repository relative include paths
REP_INC_DIR += include/arm_v5

#
# Configure target CPU
#
CC_MARCH += -march=armv5

include $(call select_from_repositories,mk/spec-arm.mk)

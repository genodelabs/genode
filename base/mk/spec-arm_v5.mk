SPECS += arm

#
# Configure target CPU
#
CC_MARCH += -march=armv5

include $(call select_from_repositories,mk/spec-arm.mk)

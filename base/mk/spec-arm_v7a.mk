SPECS += arm

#
# Configure target CPU
#
CC_MARCH += -march=armv7-a

include $(call select_from_repositories,mk/spec-arm.mk)

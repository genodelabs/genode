SPECS += foc_arm rpi

include $(call select_from_repositories,mk/spec/rpi.mk)
include $(call select_from_repositories,mk/spec/foc_arm.mk)

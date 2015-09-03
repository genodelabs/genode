SPECS += foc_arm arndale

include $(call select_from_repositories,mk/spec/fpu_vfpv3.mk)
include $(call select_from_repositories,mk/spec/arndale.mk)
include $(call select_from_repositories,mk/spec/foc_arm.mk)

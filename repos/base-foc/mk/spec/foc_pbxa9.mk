SPECS += foc_arm pbxa9

include $(call select_from_repositories,mk/spec/pbxa9.mk)
include $(call select_from_repositories,mk/spec/foc_arm.mk)

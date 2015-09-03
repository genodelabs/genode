SPECS += foc_arm panda

include $(call select_from_repositories,mk/spec/panda.mk)
include $(call select_from_repositories,mk/spec/foc_arm.mk)

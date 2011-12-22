#
# Specifics for Codezero on ARMv5
#

SPECS += codezero_arm

CC_OPT += -D__SUBARCH__=v5

include $(call select_from_repositories,mk/spec-codezero_arm.mk)

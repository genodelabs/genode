#
# Pull in CPU specifics
#
SPECS += cortex_a9 arm_v7a

include $(call select_from_repositories,mk/spec-cortex_a9.mk)

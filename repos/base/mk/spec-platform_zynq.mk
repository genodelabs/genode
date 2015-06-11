#
# Pull in CPU specifics
#
SPECS += zynq

include $(call select_from_repositories,mk/spec-zynq.mk)

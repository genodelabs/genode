#
# Pull in CPU specifics
#
SPECS += zynq cadence_gem

include $(call select_from_repositories,mk/spec-zynq.mk)

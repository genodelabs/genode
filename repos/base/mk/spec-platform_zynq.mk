#
# Pull in CPU specifics
#
SPECS += zynq cadence_gem


REP_INC_DIR += include/platform/zynq_qemu

include $(call select_from_repositories,mk/spec-zynq.mk)

#
# Pull in CPU specifics
#
SPECS += zynq cadence_gem zynq_uart0

REP_INC_DIR += include/spec/zynq_qemu
REP_INC_DIR += include/spec/xilinx

include $(call select_from_repositories,mk/spec/zynq.mk)

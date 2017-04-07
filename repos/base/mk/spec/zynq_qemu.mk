#
# Pull in CPU specifics
#
SPECS += zynq cadence_gem

REP_INC_DIR += include/spec/zynq_qemu
REP_INC_DIR += include/spec/xilinx

include $(BASE_DIR)/mk/spec/zynq.mk

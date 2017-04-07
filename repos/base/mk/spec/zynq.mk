#
# Pull in CPU specifics
#
SPECS += cortex_a9 arm_v7a

REP_INC_DIR += include/spec/zynq

include $(BASE_DIR)/mk/spec/cortex_a9.mk

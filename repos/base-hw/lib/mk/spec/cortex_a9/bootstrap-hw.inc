INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/smp
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/arm_gic
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/cortex_a9

SRC_CC  += core/spec/arm_gic/pic.cc
SRC_CC  += core/spec/cortex_a9/board.cc
SRC_CC  += bootstrap/spec/cortex_a9/platform.cc

include $(BASE_DIR)/../base-hw/lib/mk/spec/arm_v7/bootstrap-hw.inc

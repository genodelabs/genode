INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/cortex_a8
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/imx53
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/imx

SRC_CC  += core/spec/imx53/pic.cc
SRC_CC  += bootstrap/spec/imx53/platform.cc

include $(BASE_DIR)/../base-hw/lib/mk/spec/arm_v7/bootstrap-hw.inc

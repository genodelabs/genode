INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/zynq

SRC_CC  += bootstrap/spec/zynq/platform.cc

include $(BASE_DIR)/../base-hw/lib/mk/spec/cortex_a9/bootstrap-hw.inc

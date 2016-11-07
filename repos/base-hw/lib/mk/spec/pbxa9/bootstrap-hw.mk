INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/pbxa9
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/pl011

SRC_CC  += bootstrap/spec/pbxa9/platform.cc

include $(BASE_DIR)/../base-hw/lib/mk/spec/cortex_a9/bootstrap-hw.inc

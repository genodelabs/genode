INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/imx6
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/imx

SRC_CC  += bootstrap/spec/imx6/platform.cc

NR_OF_CPUS = 4

include $(BASE_DIR)/../base-hw/lib/mk/spec/cortex_a9/bootstrap-hw.inc

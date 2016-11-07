INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/panda
INC_DIR += $(BASE_DIR)/../base-hw/src/core/include/spec/tl16c750

SRC_CC  += bootstrap/spec/panda/platform.cc

NR_OF_CPUS = 2

include $(BASE_DIR)/../base-hw/lib/mk/spec/cortex_a9/bootstrap-hw.inc

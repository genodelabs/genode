INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/arndale

SRC_CC  += bootstrap/spec/arm/cortex_a15_cpu.cc
SRC_CC  += bootstrap/spec/arndale/pic.cc
SRC_CC  += bootstrap/spec/arndale/platform.cc
SRC_CC  += hw/spec/arm/arm_v7_cpu.cc
SRC_S   += bootstrap/spec/arm/crt0.s

NR_OF_CPUS = 2

include $(REP_DIR)/lib/mk/bootstrap-hw.inc

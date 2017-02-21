INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/odroid_xu

SRC_CC  += bootstrap/spec/arm/cortex_a15_cpu.cc
SRC_CC  += bootstrap/spec/arm/pic.cc
SRC_CC  += bootstrap/spec/odroid_xu/platform.cc
SRC_CC  += hw/spec/arm/arm_v7_cpu.cc
SRC_S   += bootstrap/spec/arm/crt0.s

include $(REP_DIR)/lib/mk/bootstrap-hw.inc

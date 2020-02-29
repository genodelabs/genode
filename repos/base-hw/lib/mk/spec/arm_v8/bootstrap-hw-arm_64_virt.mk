INC_DIR += $(REP_DIR)/src/bootstrap/spec/arm_v8
INC_DIR += $(REP_DIR)/src/bootstrap/spec/arm_64_virt

SRC_CC  += bootstrap/spec/arm/gicv3.cc
SRC_CC  += bootstrap/spec/arm_64/cortex_a53_mmu.cc
SRC_CC  += bootstrap/spec/arm_64_virt/platform.cc
SRC_CC  += lib/base/arm_64/kernel/interface.cc
SRC_CC  += spec/64bit/memory_map.cc
SRC_S   += bootstrap/spec/arm_64/crt0.s

vpath spec/64bit/memory_map.cc $(BASE_DIR)/../base-hw/src/lib/hw

NR_OF_CPUS = 4

include $(REP_DIR)/lib/mk/bootstrap-hw.inc

INC_DIR += $(REP_DIR)/src/bootstrap/spec/imx8q_evk

SRC_CC  += bootstrap/spec/arm/gicv3.cc
SRC_CC  += bootstrap/spec/arm_64/cortex_a53_mmu.cc
SRC_CC  += bootstrap/spec/imx8q_evk/platform.cc
SRC_CC  += lib/base/arm_64/kernel/interface.cc
SRC_CC  += spec/64bit/memory_map.cc
SRC_S   += bootstrap/spec/arm_64/crt0.s

NR_OF_CPUS = 4

vpath spec/64bit/memory_map.cc $(REP_DIR)/src/lib/hw

include $(REP_DIR)/lib/mk/bootstrap-hw.inc

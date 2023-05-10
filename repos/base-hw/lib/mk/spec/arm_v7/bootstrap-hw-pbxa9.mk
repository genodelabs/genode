REP_INC_DIR += src/bootstrap/board/pbxa9

SRC_CC  += bootstrap/spec/arm/cpu.cc
SRC_CC  += bootstrap/spec/arm/cortex_a9_mmu.cc
SRC_CC  += bootstrap/spec/arm/gicv2.cc
SRC_CC  += bootstrap/board/pbxa9/platform.cc

include $(call select_from_repositories,lib/mk/spec/arm_v7/bootstrap-hw.inc)

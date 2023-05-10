REP_INC_DIR += src/bootstrap/board/wand_quad

SRC_CC  += bootstrap/spec/arm/cpu.cc
SRC_CC  += bootstrap/spec/arm/cortex_a9_mmu.cc
SRC_CC  += bootstrap/spec/arm/gicv2.cc
SRC_CC  += bootstrap/spec/arm/imx6_platform.cc

include $(call select_from_repositories,lib/mk/spec/arm_v7/bootstrap-hw.inc)

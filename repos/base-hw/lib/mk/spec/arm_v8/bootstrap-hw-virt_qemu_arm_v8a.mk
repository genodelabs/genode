REP_INC_DIR += src/bootstrap/board/virt_qemu_arm_v8a

SRC_CC  += bootstrap/spec/arm/gicv3.cc
SRC_CC  += bootstrap/spec/arm_64/cortex_a53_mmu.cc
SRC_CC  += bootstrap/board/virt_qemu_arm_v8a/platform.cc

include $(call select_from_repositories,lib/mk/spec/arm_v8/bootstrap-hw.inc)

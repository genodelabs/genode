REP_INC_DIR += src/bootstrap/board/virt_qemu_arm_v8a

SRC_CC  += bootstrap/spec/arm/gicv3.cc
SRC_CC  += bootstrap/spec/arm_64/cortex_a53_mmu.cc
SRC_CC  += bootstrap/board/virt_qemu_arm_v8a/platform.cc
SRC_CC  += lib/base/arm_64/kernel/interface.cc
SRC_CC  += spec/64bit/memory_map.cc
SRC_S   += bootstrap/spec/arm_64/crt0.s

NR_OF_CPUS = 4

vpath spec/64bit/memory_map.cc $(call select_from_repositories,src/lib/hw)

include $(call select_from_repositories,lib/mk/bootstrap-hw.inc)

REP_INC_DIR += src/bootstrap/board/virt_qemu_arm_v7a

SRC_CC  += bootstrap/board/virt_qemu_arm_v7a/platform.cc
SRC_CC  += bootstrap/spec/arm/cortex_a15_cpu.cc
SRC_CC  += bootstrap/spec/arm/gicv2.cc

include $(call select_from_repositories,lib/mk/spec/arm_v7/bootstrap-hw.inc)

CC_MARCH = -march=armv7ve+nofp -mtune=cortex-a15 -mfpu=vfpv3

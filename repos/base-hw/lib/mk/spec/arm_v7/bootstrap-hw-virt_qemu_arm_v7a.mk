REP_INC_DIR += src/bootstrap/board/virt_qemu_arm_v7a

SRC_CC  += bootstrap/board/virt_qemu_arm_v7a/platform.cc
SRC_CC  += bootstrap/spec/arm/arm_v7_cpu.cc
SRC_CC  += bootstrap/spec/arm/cortex_a15_cpu.cc
SRC_CC  += bootstrap/spec/arm/gicv2.cc
SRC_CC  += hw/spec/32bit/memory_map.cc
SRC_S   += bootstrap/spec/arm/crt0.s

NR_OF_CPUS = 2

CC_MARCH = -march=armv7ve -mtune=cortex-a15 -mfpu=vfpv3 -mfloat-abi=softfp

include $(call select_from_repositories,lib/mk/bootstrap-hw.inc)

NR_OF_CPUS = 2

INC_DIR   += $(BASE_DIR)/../base-hw/src/bootstrap/spec/panda
SRC_CC    += bootstrap/spec/arm/cpu.cc
SRC_CC    += bootstrap/spec/arm/cortex_a9_mmu.cc
SRC_CC    += bootstrap/spec/arm/pic.cc
SRC_CC    += bootstrap/spec/panda/platform.cc
SRC_CC    += hw/spec/arm/arm_v7_cpu.cc
SRC_CC    += hw/spec/32bit/memory_map.cc
SRC_S     += bootstrap/spec/arm/crt0.s

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc

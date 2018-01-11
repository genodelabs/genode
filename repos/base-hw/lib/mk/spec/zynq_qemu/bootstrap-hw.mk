INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/zynq_qemu

SRC_S   += bootstrap/spec/arm/crt0.s

SRC_CC  += bootstrap/spec/arm/cpu.cc
SRC_CC  += bootstrap/spec/arm/cortex_a9_mmu.cc
SRC_CC  += bootstrap/spec/arm/pic.cc
SRC_CC  += bootstrap/spec/zynq/platform.cc
SRC_CC  += hw/spec/arm/arm_v7_cpu.cc
SRC_CC  += hw/spec/32bit/memory_map.cc

NR_OF_CPUS = 1

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc

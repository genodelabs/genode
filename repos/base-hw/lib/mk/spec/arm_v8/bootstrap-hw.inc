CC_MARCH  = -march=armv8-a -mstrict-align

SRC_CC   += bootstrap/platform_cpu_memory_area.cc
SRC_CC   += lib/base/arm_64/kernel/interface.cc
SRC_S    += bootstrap/spec/arm_64/crt0.s

ARCH_WIDTH_PATH := spec/64bit

include $(call select_from_repositories,lib/mk/bootstrap-hw.inc)

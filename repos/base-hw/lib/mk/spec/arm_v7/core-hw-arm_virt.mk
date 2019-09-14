INC_DIR += $(REP_DIR)/src/core/spec/arm_virt

# add C++ sources
SRC_CC += kernel/vm_thread_off.cc
SRC_CC += platform_services.cc
SRC_CC += spec/arm/generic_timer.cc
SRC_CC += spec/arm/gicv2.cc

NR_OF_CPUS = 2

CC_MARCH = -march=armv7ve -mtune=cortex-a15 -mfpu=vfpv3 -mfloat-abi=soft

include $(REP_DIR)/lib/mk/spec/cortex_a15/core-hw.inc

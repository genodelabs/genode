INC_DIR += $(REP_DIR)/src/core/spec/virt_qemu
INC_DIR += $(REP_DIR)/src/core/spec/arm/virtualization

# add C++ sources
SRC_CC += kernel/vm_thread_on.cc
SRC_CC += spec/arm/generic_timer.cc
SRC_CC += spec/arm/gicv2.cc
SRC_CC += spec/arm_v7/virtualization/kernel/vm.cc
SRC_CC += spec/arm/virtualization/gicv2.cc
SRC_CC += spec/arm/virtualization/platform_services.cc
SRC_CC += spec/arm/virtualization/vm_session_component.cc
SRC_CC += vm_session_common.cc
SRC_CC += vm_session_component.cc

SRC_S += spec/arm_v7/virtualization/exception_vector.s

NR_OF_CPUS = 2

CC_MARCH = -march=armv7ve -mtune=cortex-a15 -mfpu=vfpv3 -mfloat-abi=soft

include $(REP_DIR)/lib/mk/spec/cortex_a15/core-hw.inc

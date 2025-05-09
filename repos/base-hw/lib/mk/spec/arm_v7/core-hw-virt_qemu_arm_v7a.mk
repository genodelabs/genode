REP_INC_DIR += src/core/board/virt_qemu_arm_v7a
REP_INC_DIR += src/core/spec/arm/virtualization

# add C++ sources
SRC_CC += kernel/vcpu_thread_on.cc
SRC_CC += spec/arm/generic_timer.cc
SRC_CC += spec/arm/gicv2.cc
SRC_CC += spec/arm_v7/virtualization/kernel/vcpu.cc
SRC_CC += spec/arm/virtualization/gicv2.cc
SRC_CC += spec/arm/virtualization/platform_services.cc
SRC_CC += spec/arm/virtualization/vm_session_component.cc
SRC_CC += vm_session_common.cc
SRC_CC += vm_session_component.cc

SRC_S += spec/arm_v7/virtualization/exception_vector.s

CC_MARCH = -march=armv7ve -mtune=cortex-a15 -mfpu=vfpv3 -mfloat-abi=softfp

include $(call select_from_repositories,lib/mk/spec/arm_v7/core-hw-cortex_a15.inc)
